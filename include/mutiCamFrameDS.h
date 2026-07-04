#pragma once

#include <stdio.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <system_error>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

#include "camFrameDS.h"

class MultiCamFrameDS
{
public:
    MultiCamFrameDS(int number);
    ~MultiCamFrameDS();
    // 查找相机
    std::tuple<int, std::vector<std::string>> ScanCamers();
    // 创建单个相机
    int CreateCamere(std::string ip, std::string filePath);
    // 初始化相机
    int Init();
    // 获取帧
    FRAME_DS Read();
    // 获取帧
    FRAME_DS Read_t(int time_wait = 50);
    // 设置模式,如果为1 返回的是 已经拼接的 图 , 如果为 2 返回的是 多个未拼接的图像。
    void SetMode(int mode);
    // 获取多帧
    std::vector<FRAME_DS> ReadBatch_t(int time_wait = 50);
    // 释放
    void Release();
private:
    // 持续从设备中获取frame
    void GetFrameCpu();
    void GetFrameCpuBatch();
    // 合并图片
    cv::Mat combineImg(std::vector<cv::Mat>& imageMats, int direction = 1);
    // 获取时间戳
    int get_timestamp();
private:
    int m_mode;                                 // 模式
    int m_camNum;                               // 相机数量
    std::vector<CamFrameDS*> m_cameras;         // 相机列表
    dvpCameraInfo   m_cameraInfo[16];
    std::vector<cv::Mat> m_backupImages;        // 用于存储成功帧的备份
    std::vector<long int> m_backupTimes;

    dataQueue<FRAME_DS> que;
    dataQueue<std::vector<FRAME_DS>> queBatch;
    std::vector<FRAME_DS> m_backupDsFrame;      // 用于存储成功帧的备份
    
    std::thread m_frameThread;
    std::atomic<int> m_runFlag;

    std::string m_ipAddr;
    std::string m_iniPath;
    std::map<std::string, std::string> m_ip2friendlyName;
};
