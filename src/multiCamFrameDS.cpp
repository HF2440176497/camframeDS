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

// 查找相机
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
        // 开启子线程
        m_frameThread = std::thread(&MultiCamFrameDS::GetFrameCpu, this);
    }
    else if (m_mode == 2)
    {
        m_frameThread = std::thread(&MultiCamFrameDS::GetFrameCpuBatch, this);
    }

    return 0; // 0 表示成功
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

int MultiCamFrameDS::SetMode(int mode)
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

    // 1. 先停止数据流
    for (auto cam = m_cameras.begin(); cam != m_cameras.end(); cam++)
    {
        delete (*cam);
    }

    // 2. 等待采集线程退出
    if (m_frameThread.joinable())
    {
        m_frameThread.join();
    }
}

void MultiCamFrameDS::GetFrameCpu()
{
    // 初始化备份图像和备份时间
    if (m_backupImages.empty()) 
    {
        m_backupImages.resize(m_camNum);
        m_backupTimes.resize(m_camNum, 0);
        for (int i = 0; i < m_camNum; i++) 
        {
            // 创建默认黑色图像作为初始备份
            m_backupImages[i] = cv::Mat::zeros(640, 640, CV_8UC1);
            m_backupTimes[i] = get_timestamp();
        }
    }

    while (m_runFlag == RUN)
    {
        std::vector<cv::Mat> images;
        std::vector<long int> times;

        // 1. 获取图像
        for (int i = 0; i < m_camNum; i++)
        {
            cv::Mat tmpImage;
            int ret = m_cameras[i]->getImageBuffer(tmpImage);
            if (ret == 0 && !tmpImage.empty())
            {
                int relativeTimestamp = get_timestamp();
                images.push_back(tmpImage);
                times.push_back(relativeTimestamp);
                m_backupImages[i] = tmpImage.clone(); // 更新备份帧
                m_backupTimes[i] = relativeTimestamp; // 更新备份时间
            }
            else
            {
                // 首次，使用初始化的备份
                if (m_backupImages[i].empty()) {
                    m_backupImages[i] = cv::Mat::zeros(640, 640, CV_8UC1);
                    m_backupTimes[i] = get_timestamp();
                }

                // 使用上次成功帧
                images.push_back(m_backupImages[i].clone());
                times.push_back(m_backupTimes[i]);

                LOG(WARNING) << "use backup image " << m_cameras[i]->getCameraInfo();
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

        // 2.图像转换
        // 3.图像合并成一张图片
        cv::Mat bigImg = combineImg(images);

        DLOG(INFO) << "combineImg: " << bigImg.cols << " x " << bigImg.rows << " x " << bigImg.channels();
        // 4、图像缩放
        // 5、推进队列
        FRAME_DS res;
        res.result = SUCCESS;
        res.relativeTimestamp = get_timestamp();
        res.frame = bigImg;
        que.setData(res);

        images.clear();
        times.clear();

        DLOG(INFO) << "MultiCameraHK GetImage  success" << que.size();
    }

    LOG(WARNING) << "GetFrameCpu quit !!";
}

void MultiCamFrameDS::GetFrameCpuBatch()
{
    while (m_runFlag == RUN)
    {
        std::vector<FRAME_DS> dsImages;

        // 1. 获取图像
        for (int i = 0; i < m_camNum; i++)
        {
            cv::Mat tmpImage;
            int ret = m_cameras[i]->getImageBuffer(tmpImage);
            if (ret == 0)
            {
                FRAME_DS res;
                res.result = SUCCESS;
                res.relativeTimestamp = get_timestamp();
                res.frame = tmpImage;

                dsImages.push_back(res);
                m_backupDsFrame[i] = res; // 更新备份帧
            }
            else
            {
                // 使用上次成功帧
                dsImages.push_back(m_backupDsFrame[i]);

                LOG(WARNING) << "read Frame ERROR" << m_cameras[i]->getCameraInfo();
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

        DLOG(INFO) << "MultiCameraHK GetImage  success" << queBatch.size();
    }

    LOG(WARNING) << "GetFrameCpuBatch quit !!";
}

cv::Mat MultiCamFrameDS::combineImg(std::vector<cv::Mat> &imageMats)
{
#if 0 /*纵向合并*/ 
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

	cv::Mat combinedImg = cv::Mat(combinedHeight, combinedWidth, imageMats[0].type(), cv::Scalar(0, 0, 0));

	int startX = 0;
	int startY = 0;
	for (size_t i = 0; i < imageMats.size(); i++) 
	{
		cv::Mat img = imageMats[i];
		cv::Mat roi = combinedImg(cv::Rect(startX, startY, img.cols, img.rows));
		imageMats[i].copyTo(roi);
		startY += img.rows;
	}
    
    return combinedImg;

#else /*横向合并*/
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
    cv::Mat combinedImg = cv::Mat(combinedHeight, combinedWidth, imageMats[0].type(), cv::Scalar(0, 0, 0));
    int startX = 0;
    int startY = 0;
    for (size_t i = 0; i < imageMats.size(); i++)
    {
        cv::Mat img = imageMats[i];
        cv::Mat roi = combinedImg(cv::Rect(startX, startY, img.cols, img.rows));
        imageMats[i].copyTo(roi);
        startX += img.cols;
    }
    return combinedImg;
#endif
}

int MultiCamFrameDS::get_timestamp()
{
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();

    // 将时间点转换为时间戳
    std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    return (int)timestamp.count();
}
