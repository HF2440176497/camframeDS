#include "mutiCamFrameDS.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define RECONNECT_INTERVAL_MS  1000              // 相机初始化失败重连的间隔 ms

MultiCamFrameDS::MultiCamFrameDS(int camNum)
{
    m_camNum = camNum;
    m_runFlag = RUN;
    m_rotateCode = -1;
    m_resizeW = 0;
    m_resizeH = 0;
    m_maxReconnect = 3;
    m_maxBackupCount = 3;
}

MultiCamFrameDS::~MultiCamFrameDS()
{
    LOG(INFO) << "~MultiCamFrameDS start";

    Release();

    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG(INFO) << "~MultiCamFrameDS end";
}


std::tuple<int, std::vector<std::string>> MultiCamFrameDS::ScanCamers()
{
    int number = -1;
    std::vector<std::string> ipList;

    dvpStatus status;
    dvpUint32 i, n;

    status = dvpRefresh(&n);
    if (status != DVP_STATUS_OK)
    {
        LOG(ERROR) << "Refresh fail";
        return std::make_tuple(0, std::vector<std::string>{});
    }

    number = n;

    if (status == DVP_STATUS_OK)
    {
        // 枚举最多16台相机的信息
        if (n > 16)
        {
            n = 16;
        }

        for (i = 0; i < n; i++)
        {
            // 逐个枚举出每个相机的信息
            status = dvpEnum(i, &m_cameraInfo[i]);
            if (status != DVP_STATUS_OK)
            {
                LOG(ERROR) << "Enumerate fail";
                return std::make_tuple(0, std::vector<std::string>{});
            }
            else
            {
                // 打印设备名称,可能包含 ip 地址
                LOG(INFO) << "Camera [" << i << "] LinkName is : " << m_cameraInfo[i].LinkName;
                std::string friendlyName = m_cameraInfo[i].FriendlyName;
                std::string LinkName = m_cameraInfo[i].LinkName;
                std::string ipAddress;
                // 查找最后一个连字符的位置
                size_t lastHyphenPos = LinkName.rfind('-');
                if (lastHyphenPos != std::string::npos)
                {
                    // 提取IP地址部分
                    ipAddress = LinkName.substr(lastHyphenPos + 1);
                }
                else
                {
                    LOG(ERROR) << "IP address segment not found";
                }
                m_ip2friendlyName.insert(std::pair<std::string, std::string>(ipAddress, friendlyName));

                LOG(INFO) << "Camera [" << i << "] ip: " << ipAddress << "; friendlyName: " << friendlyName;
                ipList.push_back(ipAddress);
            }
        }
    }
    else
    {
        LOG(ERROR) << "Find No Devices";
        return std::make_tuple(0, std::vector<std::string>{});
    }

    return std::make_tuple(number, ipList);
}

// 根据IP和配置文件创建相机
int MultiCamFrameDS::CreateCamera(std::string ip, std::string filePath)
{
    // ip 未找到时重新扫描: 网络抖动可能导致 ScanCamers 漏掉相机
    // 重复 ScanCamers 是安全的, insert 不会覆盖已有条目, 结果为多次扫描的并集
    if (m_ip2friendlyName.find(ip) == m_ip2friendlyName.end())
    {
        for (int retry = 1; retry <= m_maxReconnect; retry++)
        {
            LOG(WARNING) << "IP [" << ip << "] not found, rescan " << retry << "/" << m_maxReconnect;
            std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_INTERVAL_MS));
            ScanCamers();
            if (m_ip2friendlyName.find(ip) != m_ip2friendlyName.end())
            {
                break;
            }
        }
    }

    if (m_ip2friendlyName.find(ip) == m_ip2friendlyName.end())
    {
        LOG(ERROR) << "IP address not found after rescan";
        return -1;
    }

    CamFrameDS *cam = new CamFrameDS();
    std::string friendlyName = m_ip2friendlyName.at(ip);
    LOG(INFO) << " CreateCamera [ " << ip << "   " << friendlyName << " ]" << std::endl;

    // 初始化相机, 失败时重连, 最多重连 m_maxReconnect 次
    int ret = cam->Init(friendlyName, filePath);
    for (int retry = 1; ret != 0 && retry <= m_maxReconnect; retry++)
    {
        LOG(WARNING) << "Init fail, reconnect [" << ip << " " << friendlyName
                     << "] " << retry << "/" << m_maxReconnect;
        cam->Release();
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_INTERVAL_MS));
        ret = cam->Init(friendlyName, filePath);
    }

    if (ret != 0)
    {
        LOG(ERROR) << "Init fail after " << m_maxReconnect << " reconnects [ "
                   << ip << " " << friendlyName << " ]";
        // 仍保留相机对象以维持相机索引一致, 拉流时走备份图逻辑
        m_cameras.push_back(cam);
        return -1;
    }

    m_cameras.push_back(cam);

    return 0;
}

int MultiCamFrameDS::Init()
{
    if (m_mode == 1)
    {
        m_frameThread = std::thread(&MultiCamFrameDS::GetFrameCpu, this);
    }
    else if (m_mode == 2)
    {
        m_frameThread = std::thread(&MultiCamFrameDS::GetFrameCpuBatch, this);
    }
    return 0;
}

FRAME_DS MultiCamFrameDS::Read()
{
    FRAME_DS res;
    res = que.getData();
    return res;
}

FRAME_DS MultiCamFrameDS::Read_t(int time_wait)
{
    FRAME_DS res;
    res = que.getData(time_wait);
    return res;
}

void MultiCamFrameDS::SetMode(int mode)
{
    m_mode = mode;
}

void MultiCamFrameDS::SetRotate(int rotateCode)
{
    m_rotateCode = rotateCode;
}

void MultiCamFrameDS::SetResize(int width, int height)
{
    m_resizeW = width;
    m_resizeH = height;
}

void MultiCamFrameDS::SetMaxReconnect(int count)
{
    if (count < 0)
    {
        count = 0;
    }
    m_maxReconnect = count;
}

void MultiCamFrameDS::SetMaxBackupCount(int count)
{
    if (count < 1)
    {
        count = 1;
    }
    m_maxBackupCount = count;
}

cv::Mat MultiCamFrameDS::applyTransform(const cv::Mat& img)
{
    if (img.empty())
        return img;

    cv::Mat result = img;

    // 1. 先缩放
    int w = m_resizeW.load();
    int h = m_resizeH.load();
    if (w > 0 && h > 0 && (w != result.cols || h != result.rows))
    {
        cv::resize(result, result, cv::Size(w, h));
    }

    // 2. 再旋转 (语义与 cv::rotate 一致)
    int code = m_rotateCode.load();
    if (code >= 0)
    {
        cv::rotate(result, result, code);
    }

    return result;
}

std::vector<FRAME_DS> MultiCamFrameDS::ReadBatch_t(int time_wait)
{
    std::vector<FRAME_DS> res;
    res = queBatch.getData(time_wait);
    return res;
}

void MultiCamFrameDS::Release()
{
    m_runFlag = STOP;

    // 1. 先等待采集线程退出，避免线程在 m_cameras 被释放后继续访问
    if (m_frameThread.joinable())
    {
        m_frameThread.join();
    }

    // 2. 线程已退出，安全释放相机，并清空 vector 防止悬空指针被重复 delete
    for (auto cam = m_cameras.begin(); cam != m_cameras.end(); cam++)
    {
        delete (*cam);
    }
    m_cameras.clear();
}

void MultiCamFrameDS::GetFrameCpu()
{
    // 初始化备份图像和备份时间
    if (m_backupImages.empty()) 
    {
        m_backupImages.resize(m_camNum);
        m_backupTimes.resize(m_camNum, 0);
        m_backupFailCounts.resize(m_camNum, 0);
    }

    // 停止拼接输出的标志位
    bool outputSuspended = false;

    while (m_runFlag == RUN)
    {
        std::vector<cv::Mat> images;
        std::vector<long int> times;

        // 1. 获取图像
        for (int i = 0; i < m_camNum; i++)
        {
            cv::Mat cur_image;
            int ret = m_cameras[i]->getImageBuffer(cur_image);
            if (ret == 0 && !cur_image.empty())
            {
                int ts = get_timestamp();
                images.push_back(cur_image);
                times.push_back(ts);
                m_backupImages[i] = cur_image.clone();
                m_backupTimes[i] = ts;
                m_backupFailCounts[i] = 0;
            }
            else
            {
                if (m_backupImages[i].empty()) 
                {
                    m_backupImages[i] = cv::Mat::zeros(640, 640, CV_8UC1);
                    m_backupTimes[i] = get_timestamp();
                }
                images.push_back(m_backupImages[i].clone());
                times.push_back(m_backupTimes[i]);
                m_backupFailCounts[i]++;

                LOG(WARNING) << "use backup image [" << i << "], consecutive count: " << m_backupFailCounts[i];
                LOG(WARNING) << "  info: " << m_cameras[i]->getCameraInfo();
            }
        }

        if (images.size() == 0)
            continue;

        // 1.1 图像数和相机数不一致
        if (images.size() != m_camNum)
        {
            LOG(ERROR) << "camere number = " << m_camNum << " images.size()= " << images.size();
            continue;
        }

        // 1.2 判断本轮是否所有相机都正常拉流
        bool allNormal = true;
        for (int i = 0; i < m_camNum; i++)
        {
            // 连续使用备份图的次数, >0 说明当前一定异常
            if (m_backupFailCounts[i] > 0)
            {
                allNormal = false;
                break;
            }
        }

        // 1.3 恢复正常输出后，检查当前是否已经连续 max 次使用备份图
        if (!outputSuspended)
        {
            for (int i = 0; i < m_camNum; i++)
            {
                if (m_backupFailCounts[i] >= m_maxBackupCount)
                {
                    outputSuspended = true;
                    LOG(WARNING) << "camera [" << i << "] used backup image " << m_backupFailCounts[i]
                                 << " times consecutively (max " << m_maxBackupCount
                                 << "), suspend output until all cameras recover";
                    break;
                }
            }
        } else {
            // 停止输出期间，检查当前次所有相机能否正常拉流 
            if (!allNormal)
            {
                images.clear();
                times.clear();
                continue;
            }
            outputSuspended = false;
            LOG(WARNING) << "all cameras recovered, resume output";
        }

        int i = 0;
        for (const auto &img : images)
        {
            if (img.empty())
            {
                LOG(ERROR) << "frame [" << i << "] is empty";
                continue;
            }
            LOG(INFO) << "frame [" << i << "] size: " << img.cols << " x " << img.rows << " x " << img.channels();
            i++;
        }

        // 2.图像转换
        // 3.图像合并成一张图片
        cv::Mat combinedImg = combineImg(images);

        DLOG(INFO) << "combineImg: " << combinedImg.cols << " x " << combinedImg.rows << " x " << combinedImg.channels();
        // 4、旋转 + 缩放
        combinedImg = applyTransform(combinedImg);
        // 5、推进队列
        FRAME_DS res;
        res.result = SUCCESS;
        res.relativeTimestamp = get_timestamp();
        res.frame = combinedImg;
        que.setData(res);

        images.clear();
        times.clear();

        DLOG(INFO) << "MultiCamera GetImage  success" << que.size();
    }

    LOG(WARNING) << "GetFrameCpu quit";
}

void MultiCamFrameDS::GetFrameCpuBatch()
{
    if (m_backupImages.empty()) 
    {
        m_backupImages.resize(m_camNum);
        m_backupTimes.resize(m_camNum, 0);
    }

    while (m_runFlag == RUN)
    {
        std::vector<FRAME_DS> dsImages;

        // 1. 获取图像
        for (int i = 0; i < m_camNum; i++)
        {
            cv::Mat cur_image;
            int ret = m_cameras[i]->getImageBuffer(cur_image);
            if (ret == 0 && !cur_image.empty())
            {
                int ts = get_timestamp();
                FRAME_DS res;
                res.result = SUCCESS;
                res.relativeTimestamp = ts;
                res.frame = cur_image;

                dsImages.push_back(res);
                m_backupImages[i] = cur_image.clone();
                m_backupTimes[i] = ts;
            }
            else
            {
                if (m_backupImages[i].empty()) 
                {
                    m_backupImages[i] = cv::Mat::zeros(640, 640, CV_8UC1);
                    m_backupTimes[i] = get_timestamp();
                }
                FRAME_DS res;
                res.result = SUCCESS;
                res.relativeTimestamp = m_backupTimes[i];
                res.frame = m_backupImages[i].clone();

                dsImages.push_back(res);

                LOG(WARNING) << "use backup image [" << i << "]";
                LOG(WARNING) << "  info: " << m_cameras[i]->getCameraInfo();
            }
        }

        if (dsImages.size() == 0)
            continue;

        // 1.1 图像数和相机数不一致
        if (dsImages.size() != m_camNum)
        {
            LOG(ERROR) << "camere number = " << m_camNum << " images.size()= " << dsImages.size();
            continue;
        }

        // 对每帧进行旋转 + 缩放
        for (auto& ds : dsImages)
        {
            ds.frame = applyTransform(ds.frame);
        }

        queBatch.setData(dsImages);

        dsImages.clear();

        DLOG(INFO) << "MultiCamera GetImage  success" << queBatch.size();
    }

    LOG(WARNING) << "GetFrameCpuBatch quit";
}

cv::Mat MultiCamFrameDS::combineImg(std::vector<cv::Mat> &imageMats, int direction)
{
    cv::Mat combinedImg;
    if (direction == 0) {  // 纵向合并
        int combinedWidth = 0;
        int combinedHeight = 0;
        for (size_t i = 0; i < imageMats.size(); i++)
        {
            cv::Mat img = imageMats[i];
            combinedHeight += img.rows;
            if (img.cols > combinedWidth)
            {
                combinedWidth = img.cols;
            }
        }

        combinedImg = cv::Mat(combinedHeight, combinedWidth, imageMats[0].type(), cv::Scalar(0, 0, 0));

        int startX = 0;
        int startY = 0;
        for (size_t i = 0; i < imageMats.size(); i++) 
        {
            cv::Mat img = imageMats[i];
            cv::Mat roi = combinedImg(cv::Rect(startX, startY, img.cols, img.rows));
            imageMats[i].copyTo(roi);
            startY += img.rows;
        }

    } else if (direction == 1) {  // 横向合并
        int combinedWidth = 0;
        int combinedHeight = 0;
        for (size_t i = 0; i < imageMats.size(); i++)
        {
            cv::Mat img = imageMats[i];
            combinedWidth += img.cols;
            if (img.rows > combinedHeight)
            {
                combinedHeight = img.rows;
            }
        }
        combinedImg = cv::Mat(combinedHeight, combinedWidth, imageMats[0].type(), cv::Scalar(0, 0, 0));
        int startX = 0;
        int startY = 0;
        for (size_t i = 0; i < imageMats.size(); i++)
        {
            cv::Mat img = imageMats[i];
            cv::Mat roi = combinedImg(cv::Rect(startX, startY, img.cols, img.rows));
            imageMats[i].copyTo(roi);
            startX += img.cols;
        }
    } else {
        LOG(ERROR) << "direction unknown, direction = " << direction;
    }
    return combinedImg;
}

int MultiCamFrameDS::get_timestamp()
{
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();

    // 将时间点转换为时间戳
    std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    return (int)timestamp.count();
}
