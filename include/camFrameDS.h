#pragma once

#include <stdio.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <system_error>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

#include "glog/logging.h"
#include "glog/log_severity.h"

#include "dataqueue.h"
#include "DVPCamera.h"

#define SLEEPTIME   60

#define END         1
#define SUCCESS     0
#define NONE        -1
//#define ERROR     -2

#define RUN  1
#define STOP 0

#define GRABTIMEOUT   3000                  // 图像获取的超时时间 ms

typedef struct Struct_FRAME 
{
    int result = NONE;
    cv::Mat frame;
    long int relativeTimestamp;             // 当前帧相对于初始帧的毫秒时间戳
} FRAME_DS;

typedef struct _DataFormat
{
	unsigned char*			pImageData;
	dvpImageFormat		    pixelFormat;
	unsigned int            imageSize;
	unsigned int			width;
	unsigned int			height;
}DataFormat;

// 度申科技 https://www.do3think.com/

class CamFrameDS
{
public:
    CamFrameDS();
    ~CamFrameDS();
    // 初始化
    int Init(std::string ipAddr,std::string filePath);
    // 释放
    void Release();
    // 获取图片数据
    int getImageBuffer(cv::Mat& image);
    // 获取相机信息
    std::string getCameraInfo();
private:    
    // 加载配置文件，设置一些参数
    int loadParamsFile(std::string filePath);
    // 保存配置文件
    int saveParamsFile(std::string filePath);
    // 判断句柄是否有效
    bool IsValidHandle(dvpHandle handle);
    // 打开设备
    int _openDev(std::string ipAddr,std::string filePath);
    // 开始拉流
    int _startGrabbing();
    
private:
    bool create_directory(const std::string& path, std::error_code& ec);
    std::string get_save_name(std::string path, int timestamp, std::string img_type);
    bool convert2mat(dvpFrame* pFrameInfo, unsigned char* pData, cv::Mat& srcImage);
private:
    std::atomic<int> m_errornum;
    dvpHandle       m_handle;
    dvpCameraInfo   m_cameraInfo[16];
    bool            SoftTriggerFlag;
    std::string m_img_save_path = "";
    std::atomic<size_t> m_count_save;
    std::atomic<size_t> m_save_interval;
    std::string m_friendlyName;
    std::string m_iniPath;
};
