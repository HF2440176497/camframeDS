cmake_minimum_required(VERSION  3.5.0)
project(DS_INSPUR)
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "Please create a separate directory for build files.")
endif()

add_compile_options(-std=c++11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_BUILD_TYPE "Debug")
set(PYBIND11_CPP_STANDARD -std=c++11)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bdynamic")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g3 -Wl,-Bdynamic")

find_package(pybind11 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(GFLAGS REQUIRED)
find_package(GLOG REQUIRED)
find_package(Python 3.9 REQUIRED COMPONENTS Development)
message(STATUS "Python Include Dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python Libraries: ${Python_LIBRARIES}")
message(STATUS "GLOG Libraries: ${GLOG_LIBRARIES}")


include_directories(/usr/local/include/glog)

include_directories(${Python_INCLUDE_DIRS})
include_directories(${pybind11_INCLUDE_DIRS}
		    /usr/local/lib/python3.9/site-packages/numpy/core/include
                    ./include
                    ./3rdparty
                    /usr/local/include
                    /usr/local/include/opencv4
                    ${GFLAGS_INCLUDE_DIRS}
)

link_directories(${Python_LIBRARIES})
link_directories(/usr/local/lib
	               ./lib
)

add_library(multicamframeDS MODULE
            src/camFrameDS.cpp
            src/multiCamFrameDS.cpp
            src/pyMultiCamFrameDS.cpp
            3rdparty/ndarray_converter/src/ndarray_converter.cpp
)
target_link_libraries(multicamframeDS
            pybind11::module
            opencv_core
            opencv_highgui
            opencv_imgproc
            dvp
            hzd
            ${OpenCV_LIBS}
            ${GFLAGS_LIBRARIES}
            glog
)

add_executable(main src/main.cpp src/camFrameDS.cpp src/multiCamFrameDS.cpp)

target_link_libraries(main
            opencv_core
            opencv_highgui
            opencv_imgproc
            dvp
            hzd
            ${OpenCV_LIBS}
            ${GFLAGS_LIBRARIES}
            glog
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
