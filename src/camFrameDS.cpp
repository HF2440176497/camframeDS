#include "camFrameDS.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

CamFrameDS::CamFrameDS()
{
    SoftTriggerFlag = false;
}

CamFrameDS::~CamFrameDS()
{
    LOG(INFO) << "~CamFrameDS start";

    Release();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG(INFO) << "~CamFrameDS end";
}

int CamFrameDS::Init(std::string friendlyName, std::string filePath)
{
    m_friendlyName = friendlyName;
    m_iniPath = filePath;

    int ret = _openDev(m_friendlyName, filePath);
    if (ret != 0)
    {
        LOG(ERROR) << "_openDev failed!!  " << m_friendlyName;
        return ret;
    }

    ret = loadParamsFile(filePath);
    if (ret != 0)
    {
        LOG(ERROR) << "loadParamsFile failed!!  " << filePath;
        return ret;
    }

#if 0
    // 初始化曝光模式，曝光时间，模拟增益，消频闪，分辨率
    IsMonoCamera(m_handle);
    InitTrigger();
    InitAEMode();
    InitAETarget();
    InitSpinExpoTime();
    InitSpinGain();
    InitROIMode();
    InitTargetFormat();
    InitColorSolution();
#endif

    ret = _startGrabbing();
    if (ret != 0)
    {
        LOG(ERROR) << "_startGrabbing failed!! " << m_friendlyName;
        return ret;
    }

    return 0;
}

int CamFrameDS::loadParamsFile(std::string filePath)
{
    dvpStatus status;
    status = dvpLoadConfig(m_handle, filePath.c_str());
    if (status != DVP_STATUS_OK)
    {
        LOG(ERROR) << "Load ParamsFile Fail!";
        return -1;
    }

    return 0;
}

int CamFrameDS::saveParamsFile(std::string filePath)
{
    dvpStatus status;
    status = dvpSaveConfig(m_handle, filePath.c_str());
    if (DVP_STATUS_OK != status)
    {
        LOG(ERROR) << "Save ParamsFile fail!";
        return -1;
    }

    return 0;
}

void CamFrameDS::Release()
{
    // 1. 先停止数据流
    dvpStreamState state;
    dvpGetStreamState(m_handle, &state);
    if (state == STATE_STARTED)
    {
        dvpStop(m_handle);
        // 等待流真正停止
        int retry = 0;
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            dvpGetStreamState(m_handle, &state);
        } while (state != STATE_STOPED && retry++ < 100);
    }

    // 2. 关闭设备句柄
    if (IsValidHandle(m_handle))
    {
        dvpClose(m_handle);
        m_handle = 0;
    }
}

bool CamFrameDS::IsValidHandle(dvpHandle handle)
{
    bool bValidHandle;
    dvpIsValid(handle, &bValidHandle);
    return bValidHandle;
}

// Init 中调用
int CamFrameDS::_openDev(std::string friendlyName, std::string filePath)
{
    dvpStatus status;
    dvpStreamState state;

    if (!IsValidHandle(m_handle))
    {
        // 1.可以传入 设备的名称 【创建设备的句柄】
        LOG(INFO) << "[ " << friendlyName << " ]";
        const char *name = friendlyName.c_str();
        status = dvpOpenByName(name, OPEN_NORMAL, &m_handle);
        if (status != DVP_STATUS_OK)
        {
            LOG(ERROR) << "Open the camera fail!";
            return -1;
        }

#if 0
        // 初始化曝光模式，曝光时间，模拟增益，消频闪，分辨率
        IsMonoCamera(m_handle);
        InitTrigger();
        InitAEMode();
        InitAETarget();
        InitSpinExpoTime();
        InitSpinGain();
        InitROIMode();
        InitTargetFormat();
        InitColorSolution();
#endif
        // 2.获取流的状态
        status = dvpGetStreamState(m_handle, &state);
        if (status != DVP_STATUS_OK)
        {
            LOG(ERROR) << "Get the stream state fail!";
            return -1;
        }

        if (state == STATE_STARTED)
        {
            status = dvpStop(m_handle);
        }
    }
    else
    {
        // 如果图像正在采集，则调用 on_pushButton_Start_clicked 停止采集
        status = dvpGetStreamState(m_handle, &state);
        if (state == STATE_STARTED)
        {
            status = dvpStop(m_handle);
        }

        status = dvpSaveConfig(m_handle, 0);
        status = dvpClose(m_handle);

        m_handle = 0;
    }

    return 0;
}

int CamFrameDS::_startGrabbing()
{
    dvpStatus status;
    dvpStreamState state;
    bool bTrigStatus;

    if (IsValidHandle(m_handle))
    {
        status = dvpGetStreamState(m_handle, &state);
        if (status != DVP_STATUS_OK)
        {
            LOG(ERROR) << "Get the stream state fail!";
            return -1;
        }

        if (state == STATE_STOPED)
        {
            status = dvpGetTriggerState(m_handle, &bTrigStatus);
            if (status != DVP_STATUS_FUNCTION_INVALID)
            {
                // 在启动视频流之前先设置为触发模式
                status = dvpSetTriggerState(m_handle, SoftTriggerFlag ? true : false);
                if (status != DVP_STATUS_OK)
                {
                    LOG(ERROR) << "Set status of trigger fail!";
                    return -1;
                }
            }

            status = dvpStart(m_handle);
            if (status != DVP_STATUS_OK)
            {
                LOG(ERROR) << "Start the video stream fail!";
                return -1;
            }
            else
            {
                LOG(INFO) << "Start Grabbing success";
            }
        }
        else
        {
            status = dvpStop(m_handle);
            // TODO 停止线程
        }
    }

    return 0;
}

int CamFrameDS::getImageBuffer(cv::Mat &image)
{
    dvpFrame stOutFrame; // 采集到的图像的结构体指针
    void *pBuffer;       // 采集到的图像的内存首地址
    dvpStatus status;

    status = dvpGetFrame(m_handle, &stOutFrame, &pBuffer, GRABTIMEOUT);

    if (status == DVP_STATUS_OK)
    {
	    DLOG(INFO) << "dvpGetFrame success";
        //图像格式转换，并写入同步队列
        cv::Mat bgr24;
        convert2mat(&stOutFrame, (unsigned char *)pBuffer, bgr24);
        bgr24.copyTo(image);

        return 0;
    }
    
    LOG(WARNING) << "dvpGetFrame failed!! " << m_friendlyName << " " << m_iniPath;

    return -1;
}

std::string CamFrameDS::getCameraInfo()
{
    return m_friendlyName + " " + m_iniPath;
}

bool CamFrameDS::create_directory(const std::string &path, std::error_code &ec)
{
#ifdef _WIN32
    // Windows 优化实现
    DWORD attrs = ::GetFileAttributesA(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES)
    {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            ec.clear();
            return true;
        }
        ec = std::make_error_code(std::errc::file_exists);
        return false;
    }

    if (::CreateDirectoryA(path.c_str(), nullptr))
    {
        ec.clear();
        return true;
    }

    ec = std::error_code(::GetLastError(), std::system_category());
    return false;

#else
    // POSIX 实现
    struct stat st;
    if (::stat(path.c_str(), &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            ec.clear();
            return true;
        }
        ec = std::make_error_code(std::errc::file_exists);
        return false;
    }

    if (::mkdir(path.c_str(), S_IRWXU) == 0)
    {
        ec.clear();
        return true;
    }

    ec = std::error_code(errno, std::system_category());
    return false;
#endif
}

//获取图片路径，建立目录
std::string CamFrameDS::get_save_name(std::string path, int timestamp, std::string img_type)
{
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 转换为time_t以便进一步转换为tm结构
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    // 转换为tm结构
    std::tm *now_tm = std::localtime(&now_time);
    //一级目录  年月日
    std::string path1 = std::to_string(1900 + now_tm->tm_year) + "-" + std::to_string(1 + now_tm->tm_mon) + "-" + std::to_string(now_tm->tm_mday);
    path = path + "/" + path1;
    std::error_code ec;
    if (create_directory(path, ec))
    {
        LOG(ERROR) << "Directory created/exists" << std::endl;
    }
    else
    {
        LOG(ERROR) << "Error: " << ec.message() << std::endl;
    }

    //二级目录  小时
    std::string path2 = std::to_string(now_tm->tm_hour);
    path = path + "/" + path2;
    if (create_directory(path, ec))
    {
        LOG(ERROR) << "Directory created/exists" << std::endl;
    }
    else
    {
        LOG(ERROR) << "Error: " << ec.message() << std::endl;
    }
    path = path + "/" + std::to_string(timestamp) + "." + img_type;
    return path;
}

bool CamFrameDS::convert2mat(dvpFrame *pFrameInfo, unsigned char *pData, cv::Mat &srcImage)
{
    if (pFrameInfo->format == FORMAT_MONO)
    {
        srcImage = cv::Mat(pFrameInfo->iHeight, pFrameInfo->iWidth, CV_8UC1, pData);
        LOG(INFO) << "MONO convert to cv::Mat OK.";
    }
    else if (pFrameInfo->format == FORMAT_BGR24)
    {
        srcImage = cv::Mat(pFrameInfo->iHeight, pFrameInfo->iWidth, CV_8UC3, pData);
        LOG(INFO) << "BGR24 convert to cv::Mat OK.";
    }
    else
    {
        LOG(ERROR) << "unsupported pixel format";
        return false;
    }

    if (NULL == srcImage.data)
    {
        LOG(ERROR) << "Invalid data!";
        return false;
    }

    return true;
}
