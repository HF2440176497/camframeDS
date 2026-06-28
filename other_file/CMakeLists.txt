


# Python 路径配置
set(VSTREAM_PYTHON_ROOT_DIR "/usr/local/python3.12"
    CACHE PATH "Custom Python root directory (e.g. /usr/local/python3.12). If not set, use system default.")
set(VSTREAM_PYTHON_EXECUTABLE "/usr/local/bin/python3.12"
    CACHE FILEPATH "Custom Python executable (e.g. /usr/local/python3.12/bin/python3.12). If not set, use system default.")

# 静态库 and 动态库输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${VSTREAM_ROOT_PATH}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${VSTREAM_ROOT_PATH}/lib)

# 可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${VSTREAM_ROOT_PATH}/bin)

# ---[ libyuv
if (VSTREAM_BUILD_LIBYUV)
  set(LIBYUV_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(LIBYUV_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(LIBYUV_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  add_subdirectory(${VSTREAM_ROOT_PATH}/3rdparty/libyuv)
  set(VSTREAM_HAS_BUILT_LIBYUV TRUE)
endif()

# ---[ pybind11
if (VSTREAM_BUILD_PYBIND11)
  message(STATUS "Pybind11 building from 3rdparty/pybind11")

  # 如果用户指定了自定义 Python 路径，则强制使用
  if(VSTREAM_PYTHON_ROOT_DIR)
    set(Python_ROOT_DIR "${VSTREAM_PYTHON_ROOT_DIR}" CACHE PATH "Python root directory" FORCE)
    set(Python3_ROOT_DIR "${VSTREAM_PYTHON_ROOT_DIR}" CACHE PATH "Python3 root directory" FORCE)
    message(STATUS "Using custom Python_ROOT_DIR: ${VSTREAM_PYTHON_ROOT_DIR}")
  endif()
  if(VSTREAM_PYTHON_EXECUTABLE)
    set(Python_EXECUTABLE "${VSTREAM_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable" FORCE)
    set(Python3_EXECUTABLE "${VSTREAM_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python3 executable" FORCE)
    set(PYTHON_EXECUTABLE "${VSTREAM_PYTHON_EXECUTABLE}" CACHE FILEPATH "Python executable" FORCE)
    message(STATUS "Using custom Python_EXECUTABLE: ${VSTREAM_PYTHON_EXECUTABLE}")
  endif()

  set(PYBIND11_PYTHON_VERSION 3)
  add_subdirectory(${VSTREAM_ROOT_PATH}/3rdparty/pybind11)
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module Development.Embed)

  message(STATUS "VSTREAM_BUILD_PYBIND11: using Python ")
  message(STATUS "  Interpreter  : ${Python3_EXECUTABLE}")
  message(STATUS "  Version      : ${Python3_VERSION}")
  message(STATUS "  Include dir  : ${Python3_INCLUDE_DIRS}")
  message(STATUS "  Library      : ${Python3_LIBRARIES}")

  set(VSTREAM_HAS_BUILT_PYBIND11 TRUE)
endif()


# ---[ glog
include(${VSTREAM_ROOT_PATH}/cmake/FindGlog.cmake)
if(GLOG_FOUND)
  message(STATUS "Using installed GLog for modules unittest")
  list(APPEND VSTREAM_MODULES_3RDPARTY_INCLUDE ${GLOG_INCLUDE_DIRS})
  list(APPEND VSTREAM_MODULES_3RDPARTY_LIBS ${GLOG_LIBRARIES})
else()
  message(FATAL_ERROR "GLog not found! Please ensure GLog is installed and set GLOG_ROOT_DIR or CMAKE_PREFIX_PATH")
endif()


# ---[ opencv
include(${VSTREAM_ROOT_PATH}/cmake/FindOpenCV.cmake)
if(OpenCV_FOUND)
  list(APPEND VSTREAM_MODULES_3RDPARTY_INCLUDE ${OpenCV_INCLUDE_DIRS})
  list(APPEND VSTREAM_MODULES_3RDPARTY_LIBS ${OpenCV_LIBS})
else()
  message(FATAL_ERROR "OpenCV not found. Please ensure OpenCV is installed")
endif()


# ---[ pybind11
if(DEFINED VSTREAM_HAS_BUILT_PYBIND11 AND VSTREAM_HAS_BUILT_PYBIND11)
  message(STATUS "Using pybind11 from 3rdparty build")
else()
  # 系统中如果安装，使用 find_package 查找
  find_package(pybind11 CONFIG QUIET)
  if(NOT pybind11_FOUND)
    include(${VSTREAM_ROOT_PATH}/cmake/FindPybind11.cmake)
  endif()
  if(NOT pybind11_FOUND)
    message(FATAL_ERROR "Pybind11 not found. Install it or enable VSTREAM_BUILD_PYBIND11")
  endif()
endif()