#!/bin/bash

# 默认不清理且使用debug模式
CLEAN=false
BUILD_TYPE="debug"

# 解析参数
while getopts ":cr" opt; do
  case $opt in
    c)
      CLEAN=true
      ;;
    r)
      BUILD_TYPE="release"
      ;;
    \?)
      echo "无效选项: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

# 如果带-c参数或第一次构建，执行清理
if [ "$CLEAN" = true ]; then
  echo "Clearing build directories..."
  rm -Rf ./build
  rm -Rf ./install
  rm -Rf ./log
fi

# 备份CMakeLists.txt
mv CMakeLists.txt CMakeLists.txt.bak

# 根据构建类型设置mixin参数
MIXIN_ARG=""
if [ "$BUILD_TYPE" = "release" ]; then
  echo "Using Release build"
  MIXIN_ARG="--mixin release"
else
  echo "Using Debug build"
  MIXIN_ARG="--mixin debug"
fi

# 构建命令
#  -DWITH_SPDLOG=ON \
#  -DBUILD_EXE=ON \
#   --packages-select er10_700_description \
#   -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
colcon --log-level info \
  build \
  --event-handlers console_direct+ \
  --parallel-workers 16 \
  --cmake-args \
  -Wno-dev \
  -DBUILD_EXE=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \

# 恢复CMakeLists.txt
mv CMakeLists.txt.bak CMakeLists.txt
echo "Finished"