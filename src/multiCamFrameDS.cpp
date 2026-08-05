#include "mutiCamFrameDS.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

MultiCamFrameDS::MultiCamFrameDS(int camNum)
{
    m_camNum = camNum;
    m_runFlag = RUN;
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
        LOG(ERROR) << "Refresh fail!";
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
                LOG(ERROR) << "Enumerate fail!";
                return std::make_tuple(0, std::vector<std::string>{});
            }
            else
            {
                // 打印设备名称,可能包含 ip 地址
                LOG(INFO) << "Camera" << i << "LinkName is : " << m_cameraInfo[i].LinkName;
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
                    LOG(ERROR) << "IP address segment not found!";
                }
                m_ip2friendlyName.insert(std::pair<std::string, std::string>(ipAddress, friendlyName));

                LOG(INFO) << "Camera" << i << "ipAddress is : " << ipAddress << " friendlyName is : " << friendlyName;
                ipList.push_back(ipAddress);
            }
        }
    }
    else
    {
        LOG(ERROR) << "Find No Devices!";
        return std::make_tuple(0, std::vector<std::string>{});
    }

    return std::make_tuple(number, ipList);
}

// 根据IP和配置文件创建相机
int MultiCamFrameDS::CreateCamere(std::string ip, std::string filePath)
{
    CamFrameDS *cam = new CamFrameDS();

    if (m_ip2friendlyName.find(ip) == m_ip2friendlyName.end())
    {
        LOG(ERROR) << "IP address not found";
        return -1;
    }
    std::string friendlyName = m_ip2friendlyName.at(ip);
    LOG(INFO) << " CreateCamere [ " << ip << "   " << friendlyName << " ]" << std::endl;

    cam->Init(friendlyName, filePath);
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
    }

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

                LOG(WARNING) << "use backup image [" << i << "]";
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
        // 4、图像缩放
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
