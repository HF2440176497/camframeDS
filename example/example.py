import libmulticamframeDS
import cv2
import time
import os

def main():
    # 初始化参数
    camera_num = 2
    
    config_file_2_155 = "2.155.ini"
    config_file_3_155 = "3.155.ini"
    
    output_dir = "captured_frames"
    os.makedirs(output_dir, exist_ok=True)

    try:
        # 1. 创建相机管理器
        mgr = libmulticamframeDS.MultiCamFrameDS(camera_num)
        print(f"相机数量为: {camera_num}")

        # 2. 扫描网络相机
        number, ip_list = mgr.scan_cameras()
        if number == 0:
            print("No cameras found or scan failed.")
        else:
            print(f"Found {number} cameras: {ip_list}")

        print(f"发现 {len(ip_list)} 个相机: {ip_list}")


        ip_2_155="192.168.2.155"
        # 3. 创建并初始化相机,如果有多个相机,需要传入对应的ip地址和配置文件路径！！！
        if mgr.create_camera(ip_2_155, config_file_2_155) == 0:
            print(f"已创建相机: {ip_2_155}")
        else:
            print(f"创建失败: {ip_2_155}")

        ip_3_155="192.168.3.155"
        if mgr.create_camera(ip_3_155, config_file_3_155) == 0:
            print(f"已创建相机: {ip_3_155}")
        else:
            print(f"创建失败: {ip_3_155}")
            
        mgr.setMode(1)

        # 4. 初始化相机系统
        if mgr.init() != 0:
            print("初始化失败")
            return

        # 5. 采集图像（10次尝试）
        print("\n开始采集...")
        for i in range(10):
            frame = mgr.read_t(50)  # 50ms超时
            if frame.result == 0:
                filename = f"{output_dir}/frame_{i}_{int(time.time())}.jpg"
                cv2.imwrite(filename, frame.frame)
                print(f"保存成功: {filename} | 时间戳: {frame.relativeTimestamp}ms")
            else:
                print(f"第{i}次采集失败")
            time.sleep(0.1)

        if 'mgr' in locals():
            mgr.release()

        print("\n已释放所有资源")
    except Exception as e:
        print(f"运行异常: {str(e)}")

if __name__ == "__main__":
    main()
