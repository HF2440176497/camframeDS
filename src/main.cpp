#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <opencv2/opencv.hpp>
#include "mutiCamFrameDS.h"

int main() {
    // 初始化参数
    int camera_num = 1;
    std::string config_file = "2.155.ini";

    try {
        // 1. 创建相机管理器
        MultiCamFrameDS* mgr = new MultiCamFrameDS(camera_num);
        std::cout << "相机数量为: " << camera_num << std::endl;

        // 2. 扫描网络相机
        auto [number, ip_list] = mgr->ScanCamers();
        if (number == 0) {
            std::cerr << "错误: 未发现任何相机" << std::endl;
            throw std::runtime_error("未发现相机");
        }

        std::cout << "发现 " << number << " 个相机: ";
        for (const auto& ip : ip_list) {
            std::cout << ip << " ";
        }
        std::cout << std::endl;

        std::string ip = "192.168.2.155";
        // 3. 创建并初始化相机
        if (mgr->CreateCamere(ip.c_str(), config_file.c_str()) == 0) {
            std::cout << "已创建相机: " << ip << std::endl;
        } else {
            std::cout << "创建失败: " << ip << std::endl;
        }

        mgr->SetMode(1);

        // 4. 初始化相机系统
        if (mgr->Init() != 0) {
            std::cout << "初始化失败" << std::endl;
            delete mgr;
            return -1;
        }

        // 5. 采集图像
        std::cout << "\n开始采集..." << std::endl;
        for (int i = 0; i < 1000; ++i) {
            FRAME_DS frame = mgr->Read_t(50);  // 50ms超时
            if (frame.result == 0) {
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::system_clock::to_time_t(now);
                std::string filename = "frame_" + std::to_string(i) + 
                                      "_" + std::to_string(timestamp) + ".jpg";
                cv::imwrite(filename, frame.frame);
                std::cout << "保存成功: " << filename << " | 时间戳: " 
                          << frame.relativeTimestamp << "ms" << std::endl;
            } else {
                std::cout << "第" << i << "次采集失败" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    } catch (const std::exception& e) {
        
	std::cerr << "运行异常: " << e.what() << std::endl;

    	//if (mgr != nullptr) {
        //	mgr->Release();
        //	delete mgr;
        //	std::cout << "\n已释放所有资源" << std::endl;
    	//}
    }

    return 0;
}

