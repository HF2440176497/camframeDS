# DS_INSPUR 编译与打包说明

## 一、整体架构概览

项目采用 CMake 构建系统，通过 shell 脚本封装编译和打包命令，最终使用 CPack 生成 DEB 安装包。

```
build.sh / package.sh
       ↓
     CMake
       ↓
  编译 + 链接 (pybind11 module)
       ↓
  make install → 安装到 /opt/software/DS_INSPUR
       ↓
  CPack (DEB) → 生成 .deb 安装包
```

---

## 二、编译流程详解

### 2.1 编译入口脚本 [build.sh](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/build.sh)

支持两种构建模式：

```bash
./build.sh -debug     # Debug 模式
./build.sh -release   # Release 模式
```

脚本执行步骤：
1. 创建构建目录（`build_debug` 或 `build_release`）
2. 运行 CMake 配置，指定安装前缀 `-DCMAKE_INSTALL_PREFIX=/opt/software/DS_INSPUR`
3. 执行 `make -j4` 编译
4. 执行 `make install` 安装到指定目录

### 2.2 CMake 构建配置 [CMakeLists.txt](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/CMakeLists.txt)

#### 编译目标

项目生成两个目标：

| 目标 | 类型 | 说明 |
|------|------|------|
| `multicamframeDS` | pybind11 module | Python 可导入的扩展模块，安装到 `lib/` |
| `main` | 可执行文件 | C++ 测试入口，不参与 install |

#### 依赖库查找方式

项目使用自定义 Find 模块（位于 [cmake/](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/cmake) 目录）查找第三方依赖：

- **GFlags / GLog**：通过自定义 `FindGFlags.cmake`、`FindGlog.cmake` 查找
- **OpenCV**：通过自定义 `FindOpenCV.cmake` 查找，支持 `OpenCV_ROOT_DIR` 环境变量指定路径
- **dvp / hzd**：通过 `find_library` 在 `/usr/local/lib` 和项目 `lib/` 目录下查找

#### 链接方式

所有依赖库以**动态链接**方式链接（`-Wl,-Bdynamic`），因此运行时需要这些 .so 文件存在。

---

## 三、打包流程详解

### 3.1 打包入口脚本 [package.sh](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package.sh)

```bash
./package.sh -debug     # 打包 Debug 版本
./package.sh -release   # 打包 Release 版本
```

脚本执行步骤：
1. 赋予 package/scripts/ 下脚本执行权限
2. 进入对应构建目录
3. 运行 CMake 重新配置（确保参数一致）
4. 执行 `cmake --build . --target package` 调用 CPack 生成 DEB 包

### 3.2 CPack DEB 打包配置 [package/CMakeLists.txt](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package/CMakeLists.txt)

#### 核心打包逻辑：自动收集第三方依赖

打包时通过 `ldd` 命令动态分析编译产物的依赖库，并将匹配的第三方库一同打入 DEB 包。

**流程如下：**

```
1. 查找编译产物 (libcamframeDS*.so)
        ↓
2. ldd 分析所有动态依赖
        ↓
3. 用 egrep 筛选"需要打包"的库（白名单模式）
        ↓
4. 过滤掉"系统库"（黑名单模式）
        ↓
5. 解析符号链接，取真实文件路径
        ↓
6. install 到 dev/ 目录，随 DEB 包发布
```

#### 白名单：会被打包进 DEB 的第三方库

`package/CMakeLists.txt` 第 39-40 行定义了哪些库会被收集：

```
crypto, z, avcodec, avformat, avutil, glog, gflag, event, opencv,
swscale, swresample, x264, x265, gtk, cairo, gobject, glib, jpeg,
png16, vpx, webpmux, webp, snappy, mp3lame, openjp2, opus, shine,
speex, theoraenc, theoradec, twolame, vorbis, vorbisenc, wavpack,
xvidcore, modplug, bluray, rtmp, ssh, soxr
```

**结论：OpenCV 会被包含在 DEB 包中。** 所有名字匹配 `opencv` 的依赖库（如 libopencv_core.so、libopencv_imgproc.so 等）都会被自动收集并打入 DEB 包。

#### 黑名单：不会被打包的系统库

`package/CMakeLists.txt` 第 70 行定义了排除的系统库：

```
libc.so, librt.so, libm.so, libgcc, libpthread, libdl,
FormatConversion, MediaProcess, MvCameraControl, MVGigEVisionSDK,
MVRender, libMVRender
```

这些库假定目标系统已存在，不随包分发。

### 3.3 DEB 包内容结构

安装后目录结构（以 `/opt/software/DS_INSPUR` 为根）：

```
/opt/software/DS_INSPUR/
├── lib/                      # 主库目录
│   ├── multicamframeDS*.so   # pybind11 扩展模块（编译产物）
│   ├── libdvp.so             # 项目自带第三方库（lib/ 目录下的）
│   ├── libhzd.so             # 项目自带第三方库
│   └── example.py            # 示例脚本
└── dev/                      # 自动收集的第三方依赖
    ├── libopencv_core.so.*   # OpenCV 相关库
    ├── libopencv_imgproc.so.*
    ├── libglog.so.*          # glog
    ├── libgflags.so.*        # gflags
    └── ...                   # 其他白名单内的依赖库
```

---

## 四、运行时库查找机制

### 4.1 安装后配置脚本 [postinst](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package/scripts/postinst)

DEB 包安装完成后，`postinst` 脚本会自动执行，完成以下工作：

#### 步骤 1：清理旧版本遗留的全局配置

旧版本会把安装目录写入 `/etc/ld.so.conf.d/DS_INSPUR.conf` 并执行 `ldconfig`。
该配置是**全机器共用**的，会让机器上所有程序都去相机库目录里找库，
容易把系统同名库（例如 `libz.so.1`）遮蔽掉。新版不再注册，
并在升级安装时主动删除遗留的 conf、重建一次缓存：

```bash
if [ -f /etc/ld.so.conf.d/DS_INSPUR.conf ]; then
    rm -f /etc/ld.so.conf.d/DS_INSPUR.conf
    ldconfig
fi
```

> 已移除旧版"扫描系统目录、删除 `dev/` 下重复库"的逻辑：它只扫目录第一层、
> 只扫 `dev/`，在 Ubuntu 多架构布局下基本失效。系统级库改为**不随包分发**，
> 由 deb 的 `Depends`（`zlib1g`、`libpng16-16`、`libjpeg-turbo8` 等）保证目标机已安装。

#### 步骤 2：部署前置检查

检查 `ldconfig -p` 里是否有 `libz.so.1`、`libpng16.so.16`、`libstdc++.so.6`、
`libglog.so.2`、`libgflags.so.2.3`，缺失只告警，不阻断安装。
**不做任何 `/etc/ld.so.conf.d` 注册，也不修改全局动态链接器缓存。**

### 4.2 程序运行时如何找到库

当 Python 导入 `multicamframeDS` 模块时：

1. 打包阶段已通过 `patchelf` 给模块和随包私有库写入 `$ORIGIN` / `$ORIGIN/../dev`
   优先目录（模块装在 `lib/`，自动收集的第三方库装在 `dev/`）。
   `$ORIGIN` 在运行时被替换为"该 `.so` 文件所在目录"，所以私有库只在
   本包目录内互相查找，**不会进入全机器清单、不影响其它程序**。

2. 系统级库（`libz.so.1`、`libpng16.so.16`、`libstdc++.so.6` 等）由操作系统提供，
   按加载器默认规则从系统路径解析；包内不再携带这些库的副本。

---

## 五、关键文件速查

| 文件 | 作用 |
|------|------|
| [build.sh](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/build.sh) | 编译脚本入口 |
| [package.sh](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package.sh) | 打包脚本入口 |
| [CMakeLists.txt](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/CMakeLists.txt) | 主 CMake 配置，定义编译目标 |
| [package/CMakeLists.txt](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package/CMakeLists.txt) | CPack 配置，定义 DEB 打包逻辑 |
| [package/scripts/postinst.in](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/package/scripts/postinst.in) | DEB 安装后脚本模板（含 @CMAKE_INSTALL_PREFIX@ 变量） |
| [cmake/FindOpenCV.cmake](file:///home/sasha/develop/camera/pybind11_multi_cam_DS/cmake/FindOpenCV.cmake) | OpenCV 查找模块 |
