
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
    COMMAND egrep "multicamframeDS"
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
    COMMAND egrep "crypto|libz\\.so|avcodec|avformat|avutil|glog|gflag|event|opencv|swscale|swresample|x264|x265|gtk|cairo|gobject|glib|jpeg|png16|vpx|webpmux|webp|snappy|mp3lame|openjp2|opus|shine|speex|
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
# 
# 黑名单：名单上的库一律不随包分发：由目标机 OS 提供，避免同名库遮蔽系统版本。
# 按"库名"排除，不按路径排除（构建机路径不可靠）；[.] / [+] 表示字面字符。
set(SYSTEMLIBS
    ".*libc.so"
    ".*librt.so"
    ".*libm.so"
    ".*libgcc.*"
    ".*libpthread.*"
    ".*libdl.*"
    ".*libz[.]so.*"
    ".*libpng16.*"
    ".*libjpeg.*"
    ".*libtiff.*"
    ".*libcrypto[.]so.*"
    ".*libssl[.]so.*"
    ".*libstdc[+][+].*"
    ".*libgomp.*"
    ".*libglib-2[.]0.*"
    ".*libgobject-2[.]0.*"
    ".*libcairo.*"
    ".*libgtk.*"
    ".*libX11.*"
    ".*libxcb.*"
    ".*libudev.*"
    ".*FormatConversion.*"
    ".*MediaProcess.*"
    ".*MvCameraControl.*"
    ".*MVGigEVisionSDK.*"
    ".*MVRender.*"
    ".*libMVRender.*"
)

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
    "${CMAKE_SOURCE_DIR}/depends/*.so"
    "${CMAKE_SOURCE_DIR}/lib/*.so" 
    "${CMAKE_SOURCE_DIR}/lib/*.so.*"  # 包含带版本号的库
)

install(FILES ${SO_FILES} DESTINATION ${INSTALL_LIBDIR})
install(FILES ${PROJECT_SOURCE_DIR}/example/example.py DESTINATION ${INSTALL_LIBDIR})

# 随包私有库只在本包目录内互相解析：不再依赖 /etc/ld.so.conf.d 全局注册。
# install(CODE ...) 里的代码在"安装/打包时"才执行；\${} / \$ORIGIN 前的反斜杠
# 是为了把它们留到安装阶段再展开（配置阶段展开会拿到错误的值）。
# 这里给 lib/ 与 dev/ 下的私有库统一写入 $ORIGIN:$ORIGIN/../dev；
# 模块本身由 install(TARGETS) 写入同样的 INSTALL_RPATH，二者等价，不会冲突。
install(CODE "
    find_program(PATCHELF_EXECUTABLE patchelf QUIET)
    if(PATCHELF_EXECUTABLE)
        message(STATUS \"Found patchelf: \${PATCHELF_EXECUTABLE}\")
        file(GLOB PREBUILT_LIBS
            \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_LIBDIR}/*.so*\"
            \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_LIBDIR}/../dev/*.so*\")
        foreach(LIB \${PREBUILT_LIBS})
            execute_process(
                COMMAND \${PATCHELF_EXECUTABLE} --set-rpath \"\$ORIGIN:\$ORIGIN/../dev\" \"\${LIB}\"
                RESULT_VARIABLE PATCH_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
            if(NOT PATCH_RESULT EQUAL 0)
                message(WARNING \"  Failed to patch RPATH: \${LIB}\")
            endif()
        endforeach()
    else()
        message(FATAL_ERROR \"patchelf not found - install patchelf and rebuild the package; bundled libs cannot resolve each other without it\")
    endif()
")

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

# 系统级库由目标机 OS 提供，dpkg 安装时检查（替代 postinst 里失效的"删除重复库"逻辑）。
# 清单按打包时 ldd 的真实结果推导（见 REMOVELIBS 输出）：
#   libc.so.6 / libm.so.6 / libpthread.so.0 / librt.so.1 / libdl.so.2 -> libc6
#   libstdc++.so.6 -> libstdc++6,  libgcc_s.so.1 -> libgcc-s1,  libz.so.1 -> zlib1g
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6, libstdc++6, libgcc-s1, zlib1g")

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
