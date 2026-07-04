cmake_minimum_required(VERSION  3.5.0)
project(DS_INSPUR)
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "Please create a separate directory for build files.")
endif()

set(DS_INSTALL_PREFIX "/usr/local/DS_INSPUR" CACHE PATH "Unified installation prefix for DS_INSPUR")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${DS_INSTALL_PREFIX}" CACHE PATH "Install path prefix" FORCE)
endif()
message(STATUS "DS_INSTALL_PREFIX: ${DS_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自定义 Python 路径配置
set(DS_PYTHON_ROOT_DIR "/usr/local/python3.13" CACHE PATH "Custom Python root directory.")
set(DS_PYTHON_EXECUTABLE "/usr/local/bin/python3.13" CACHE FILEPATH "Custom Python executable.")

set(CMAKE_BUILD_TYPE "Debug")
set(PYBIND11_CPP_STANDARD -std=c++17)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bdynamic")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g3 -Wl,-Bdynamic")

# 如果指定了自定义 Python 路径，强制 pybind11 与 find_package(Python3) 使用它
if(DS_PYTHON_ROOT_DIR)
    set(Python_ROOT_DIR "${DS_PYTHON_ROOT_DIR}" CACHE PATH "Python root directory" FORCE)
    set(Python3_ROOT_DIR "${DS_PYTHON_ROOT_DIR}" CACHE PATH "Python3 root directory" FORCE)
    message(STATUS "Using custom Python_ROOT_DIR: ${DS_PYTHON_ROOT_DIR}")
endif()
if(DS_PYTHON_EXECUTABLE)
    set(Python_EXECUTABLE "${DS_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable" FORCE)
    set(Python3_EXECUTABLE "${DS_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python3 executable" FORCE)
    set(PYTHON_EXECUTABLE "${DS_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable" FORCE)
    message(STATUS "Using custom Python_EXECUTABLE: ${DS_PYTHON_EXECUTABLE}")
endif()

# 使用本地 pybind11 源码编译
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/pybind11)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module Development.Embed)
message(STATUS "Using Python3")
message(STATUS "  Interpreter  : ${Python3_EXECUTABLE}")
message(STATUS "  Version      : ${Python3_VERSION}")
message(STATUS "  Include dir  : ${Python3_INCLUDE_DIRS}")
message(STATUS "  Library      : ${Python3_LIBRARIES}")

# 动态获取 numpy include 目录
if(Python3_EXECUTABLE)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import numpy; print(numpy.get_include())"
        OUTPUT_VARIABLE Python3_NUMPY_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# 使用项目自定义的查找模块
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(GFLAGS_FOUND FALSE)
set(GLOG_FOUND FALSE)
set(OpenCV_FOUND FALSE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGFlags.cmake)
if(NOT GFLAGS_FOUND)
    message(FATAL_ERROR "GFlags not found! Please ensure GFlags is installed")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGlog.cmake)
if(NOT GLOG_FOUND)
    message(FATAL_ERROR "GLog not found! Please ensure GLog is installed")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindOpenCV.cmake)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Please ensure OpenCV is installed")
endif()

message(STATUS "GFLAGS Libraries: ${GFLAGS_LIBRARIES}")
message(STATUS "GLOG Libraries: ${GLOG_LIBRARIES}")

# 显式查找项目依赖的非 CMake 第三方库，避免全局 link_directories
find_library(DVP_LIBRARY NAMES dvp PATHS /usr/local/lib ${CMAKE_CURRENT_SOURCE_DIR}/lib)
find_library(HZD_LIBRARY NAMES hzd PATHS /usr/local/lib ${CMAKE_CURRENT_SOURCE_DIR}/lib)
if(NOT DVP_LIBRARY)
    message(FATAL_ERROR "dvp library not found")
endif()
if(NOT HZD_LIBRARY)
    message(FATAL_ERROR "hzd library not found")
endif()

pybind11_add_module(multicamframeDS
  src/camFrameDS.cpp
  src/multiCamFrameDS.cpp
  src/pyMultiCamFrameDS.cpp
  src/utils/common_wrapper.cpp
)

target_include_directories(multicamframeDS
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(multicamframeDS
  PRIVATE
    ${DVP_LIBRARY}
    ${HZD_LIBRARY}
    ${OpenCV_LIBS}
    ${GFLAGS_LIBRARIES}
    ${GLOG_LIBRARIES}
)

add_executable(main 
  src/main.cpp 
  src/camFrameDS.cpp 
  src/multiCamFrameDS.cpp
)

target_include_directories(main
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(main
    ${DVP_LIBRARY}
    ${HZD_LIBRARY}
    ${OpenCV_LIBS}
    ${GFLAGS_LIBRARIES}
    ${GLOG_LIBRARIES}
)

set(INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")

add_subdirectory(package)

# 指定需要安装的文件
install(
  TARGETS
    multicamframeDS
  ARCHIVE
    DESTINATION ${INSTALL_LIBDIR}
    COMPONENT lib
  RUNTIME
    DESTINATION ${INSTALL_BINDIR}
    COMPONENT bin
  LIBRARY
    DESTINATION ${INSTALL_LIBDIR}
    COMPONENT lib
)
