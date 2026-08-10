#!/bin/bash

# 文件名: build.sh
# 用法: ./build.sh [-debug|-release]
# 作者:
# Copyright (C) 2021 Inspur
# 创建于: 2022-01-17

# 使用说明：
#	./build -debug 或者 ./build -release

# 统一安装前缀（与 CMakeLists.txt 中的 DS_INSTALL_PREFIX 保持一致）
INSTALL_PREFIX="/usr/local/DS_INSPUR"

build_debug() {
  mkdir -p build_debug
  cd build_debug
  cmake ..  -DRELEASE=OFF -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
  make -j4
  make install
  echo "build debug complete"
}

build_release() {
  mkdir -p build_release
  cd build_release
  cmake ..  -DRELEASE=ON -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
  make -j4
  make install
  echo "build release complete"
}

if [[ $1 = '-debug' ]]; then
	build_debug
	exit 0
elif [[ $1 = '-release' ]]; then
	build_release
	exit 0
else
  echo "usage:./build -debug | -release"
fi
