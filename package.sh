#!/bin/bash

# 文件名: package.sh
# 用法: ./package.sh [-debug|-release]
# 作者:
# Copyright (C) 2021 Inspur
# 创建于: 2022-01-17

# 使用说明：
#	./package -debug 或者 ./package -release

package_debug() {
  chmod 755 ./package/scripts/*
  cd build_debug
  cmake .. -G"Unix Makefiles" -DRELEASE=OFF -DCMAKE_INSTALL_PREFIX=/opt/software/DS_INSPUR
  cmake --build . --target package
  echo "package debug complete!"
}

package_release() {
  chmod 755 ./package/scripts/*
  cd build_release
  cmake .. -G"Unix Makefiles" -DRELEASE=ON -DCMAKE_INSTALL_PREFIX=/opt/software/DS_INSPUR
  cmake --build . --target package
  echo "package release complete!"
}
if [[ $1 = '-debug' ]]; then
	package_debug
	exit 0
elif [[ $1 = '-release' ]]; then
	package_release
	exit 0
else
  echo "usage:./package -debug | -release"
fi
