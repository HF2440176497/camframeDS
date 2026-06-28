
# ----------------------------------------------------------------------------
#  CMake file for package
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# 指定deb包的版本号
set(MAJOR_VERSION 2025)
set(MINOR_VERSION 07)
set(PATCH_VERSION 22)


# search for dependenlibs
set(APPLICATION_NAME "")
# get ModelPipeLine
execute_process(
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/
    COMMAND ls
    COMMAND egrep "libcamframeDS"
    RESULT_VARIABLE RETRUNSTATUS
    OUTPUT_VARIABLE APPLICATION_NAME
    ERROR_VARIABLE DEPENDEN_FILES_ERROR
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
 )

# get dependencies
execute_process(
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/
    COMMAND ldd ${APPLICATION_NAME}
    COMMAND awk "{print $3}"
    COMMAND sort -u
    COMMAND egrep "crypto|z|avcodec|avformat|avutil|glog|gflag|event|opencv|swscale|swresample|x264|x265|gtk|cairo|gobject|glib|jpeg|png16|vpx|webpmux|webp|snappy|mp3lame|openjp2|opus|shine|speex|
    theoraenc|theoradec|twolame|vorbis|vorbisenc|wavpack|xvidcore|modplug|bluray|rtmp|ssh|soxr|opencv"
    # Only libs I want to copy
    RESULT_VARIABLE RETRUNSTATUS
    OUTPUT_VARIABLE DEPENDENLIBS
    ERROR_VARIABLE DEPENDEN_FLIES_ERROR
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
 )


if (${INSTALL_FILES_ERROR})
  message("----> ${DEPENDEN_FILES_ERROR}")
endif (${INSTALL_FILES_ERROR})

message(STATUS "RESULT_VARIABLE is: ${RETRUNSTATUS}")
message(STATUS "OUTPUT_VARIABLE is: ${DEPENDENLIBS}")
string(REGEX REPLACE "\n" ";" DEPENDENLIBS "${DEPENDENLIBS}")

# LEGAL_SO_FILES 是依赖的库文件

foreach (_file ${DEPENDENLIBS}) # Resolve illega so files
  #message(STATUS "_file is: ${_file}")
  IF(${_file} MATCHES ".*so.*")
    list(APPEND LEGAL_SO_FILES ${_file})
  ENDIF(${_file} MATCHES ".*so.*")
endforeach()
message(STATUS "OUTPUT_VARIABLE is: ${DEPENDENLIBS}")

# message(STATUS "LEGAL_SO_FILES is: ${LEGAL_SO_FILES}")
# filter some libs
# TODO
set(SYSTEMLIBS ".*libc.so;.*librt.so;.*libm.so;.*libgcc;.*libpthread.*;.*libdl.*;.*FormatConversion.*;.*MediaProcess.*;.*MvCameraControl.*;.*MvCameraControl.*;.*MVGigEVisionSDK.*;.*MVRender.*;.*libMVRender.*")

set(REMOVELIBS "")
foreach (_file ${LEGAL_SO_FILES})
  foreach(_syslib ${SYSTEMLIBS})
    IF(${_file} MATCHES ${_syslib})
      list(APPEND REMOVELIBS ${_file})
      break()
    ENDIF(${_file} MATCHES ${_syslib})
  endforeach(_syslib ${SYSTEMLIBS})
endforeach()
message(STATUS "REMOVELIBS is: ${REMOVELIBS}")

foreach(_removefile ${REMOVELIBS})
  list(REMOVE_ITEM LEGAL_SO_FILES ${_removefile})
endforeach(_removefile ${REMOVELIBS})


foreach (_file ${LEGAL_SO_FILES}) # Resolve links
  get_filename_component(_resolvedFile "${_file}" REALPATH)
  list (APPEND INSTALL_RESOLVED_FILES ${_resolvedFile} )
endforeach()

# <<< Install and export targets >>>
install(
  FILES
    ${INSTALL_RESOLVED_FILES}
    DESTINATION ${INSTALL_LIBDIR}/../dev
    COMPONENT dev
)

file(GLOB_RECURSE SO_FILES 
    "${CMAKE_SOURCE_DIR}/lib/*.so" 
    "${CMAKE_SOURCE_DIR}/lib/*.so.*"  # 包含带版本号的库
)

install(FILES ${SO_FILES} DESTINATION ${INSTALL_LIBDIR})
install(FILES ${PROJECT_SOURCE_DIR}/example/example.py DESTINATION ${INSTALL_LIBDIR})

# [--  package

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Inspur")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/INSTALL.md")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "multi-camera frame DS")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

set(CPACK_DEBIAN_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "support@inspur.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "inspur")

set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# 生成带安装前缀的 postinst 脚本（将 @CMAKE_INSTALL_PREFIX@ 替换为实际路径）
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/scripts/postinst.in"
  "${CMAKE_CURRENT_BINARY_DIR}/scripts/postinst"
  @ONLY
)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/scripts/postinst")

set(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
set(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
set(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")


if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Inspur")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  endif()
endif()

message(STATUS "CPack generators: ${CPACK_GENERATOR}")

include(CPack)
