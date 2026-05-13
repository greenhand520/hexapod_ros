#!/bin/bash
# ============================================================
#  lib_microros_generation.sh [-c] [-r]
#
#  -c  清理 firmware 后重新构建
#  -r  release 模式，跳过编译直接部署（前提：已有编译产物）
#
#  在 software/ 下运行
# ============================================================

set -euo pipefail

# ==================== 参数解析 ====================

CLEAN_BUILD=false
RELEASE=false

while getopts "cr" opt; do
  case $opt in
  c) CLEAN_BUILD=true ;;
  r) RELEASE=true ;;
  *)
    echo "用法: $0 [-c] [-r]"
    exit 1
    ;;
  esac
done

# ==================== 用户配置区 ====================

ROS_DISTRO="${ROS_DISTRO:-humble}"
SETUP_BRANCH="${SETUP_BRANCH:-$ROS_DISTRO}"
CUBEMX_UTILS_REPO="https://github.com/micro-ROS/micro_ros_stm32cubemx_utils.git"
CUBEMX_UTILS_BRANCH="${CUBEMX_UTILS_BRANCH:-$ROS_DISTRO}"

# 自定义消息包名（位于 software/src/ 下）
CUSTOM_MSG_PKG="hexapod_sensor_interface"

# STM32 工程中 micro-ROS 存放目录
STM32_MICROROS_DIR="Middlewares/microros"

# ==================== 路径推导 ====================

SCRIPT_DIR="$(pwd)"
PARENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CUBEMX_UTILS_DIR="$PARENT_DIR/micro_ros_stm32cubemx_utils"
STM32_PROJECT_DIR="$PARENT_DIR/hexapod_stm32f405"

if $RELEASE; then
  MICROROS_STATIC_LIBRARY_PATH="$PARENT_DIR/micro_ros_stm32cubemx_utils/microros_static_library/library_generation"
else
  MICROROS_STATIC_LIBRARY_PATH="$PARENT_DIR/micro_ros_stm32cubemx_utils/microros_static_library_ide/library_generation"
fi

MICROROS_WS="$MICROROS_STATIC_LIBRARY_PATH/microros_ws"
MICROROS_WS_SRC="$MICROROS_WS/src"
SETUP_DIR="$MICROROS_WS_SRC/micro_ros_setup"
FW_DIR="$MICROROS_WS/firmware"
FW_ROS2_DIR="$FW_DIR/mcu_ws/ros2"

# 自定义消息包源码路径
CUSTOM_MSG_SRC="$SCRIPT_DIR/src/$CUSTOM_MSG_PKG"

# firmware 中额外包目录
USER_CUSTOM_PACKAGES_DIR="$FW_DIR/mcu_ws/extra_packages"

# libmicroros 输出路径
LIB_DIR="$MICROROS_STATIC_LIBRARY_PATH/libmicroros"

# STM32 工程部署目标
DEST="$STM32_PROJECT_DIR/$STM32_MICROROS_DIR"

# ==================== 颜色输出 ====================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail() {
  echo -e "${RED}[FAIL]${NC}  $*"
  exit 1
}

# ==================== 环境检查 ====================

if $CLEAN_BUILD; then
  CLEAN_DESC="清理重建"
else
  CLEAN_DESC="增量构建"
fi

if $RELEASE; then
  BUILD_MODE="Release"
else
  BUILD_MODE="IDE"
fi

info "=========================================="
info " micro-ROS 静态库 生成 & 部署脚本"
info " 构建模式: $CLEAN_DESC / $BUILD_MODE"
info "=========================================="
echo ""
info "脚本运行目录       = $SCRIPT_DIR"
info "micro-ROS 工作空间 = $MICROROS_WS"
info "micro_ros_setup    = $SETUP_DIR"
info "libmicroros 输出   = $LIB_DIR"
info "cubemx_utils       = $CUBEMX_UTILS_DIR"
info "自定义消息包       = $CUSTOM_MSG_SRC"
info "STM32 工程         = $STM32_PROJECT_DIR"
info "部署目标           = $DEST"
echo ""

[ -f "/opt/ros/$ROS_DISTRO/setup.bash" ] || fail "ROS2 $ROS_DISTRO 未安装"

# 编译模式：完整检查
[ -d "$SCRIPT_DIR/src" ] || fail "src 目录不存在，请确认在 software/ 下运行"
[ -d "$CUSTOM_MSG_SRC" ] || fail "自定义消息包不存在: $CUSTOM_MSG_SRC"
[ -d "$STM32_PROJECT_DIR" ] || fail "STM32 工程目录不存在: $STM32_PROJECT_DIR"
ok "环境检查通过"
echo ""

# ==================== Step 1: 拉取 micro_ros_stm32cubemx_utils ====================

info "Step 1: 拉取 micro_ros_stm32cubemx_utils..."

if [ -d "$CUBEMX_UTILS_DIR" ]; then
  warn "  $CUBEMX_UTILS_DIR 已存在，跳过克隆"
else
  git clone -b "$CUBEMX_UTILS_BRANCH" \
    "$CUBEMX_UTILS_REPO" \
    "$CUBEMX_UTILS_DIR"
  ok "  micro_ros_stm32cubemx_utils 克隆完成"
fi
echo ""

# ==================== Step 2: 拉取 micro_ros_setup ====================

info "Step 2: 拉取 micro_ros_setup 到 microros_ws/src/..."

mkdir -p "$MICROROS_WS_SRC"

if [ -d "$SETUP_DIR" ]; then
  warn "  $SETUP_DIR 已存在，跳过克隆"
else
  git clone -b "$SETUP_BRANCH" \
    https://github.com/micro-ROS/micro_ros_setup.git \
    "$SETUP_DIR"
  ok "  micro_ros_setup 克隆完成"
fi
echo ""

# 检查条件
if $CLEAN_BUILD; then
  if [ -d "$FW_DIR" ]; then
    warn "  -c 指定，清理整个 firmware 目录..."
    rm -rf "$FW_DIR"
  fi
  SKIP_BUILD=false
else
  if [ -f "$FW_DIR/build/libmicroros.a" ]; then
    warn "  已有静态库，跳过构建（使用 -c 强制重建）"
    SKIP_BUILD=true
  else
    info "  firmware 中无静态库，清理整个 firmware 目录"
    rm -rf "$FW_DIR"
    SKIP_BUILD=false
  fi
fi

if ! ${SKIP_BUILD:-false}; then
  # ==================== Step 3: 编译 micro-ROS ====================
  info "Step 3: 编译 micro-ROS 工作空间..."

  cd "$MICROROS_WS"

  if ! ${RELEASE:-true}; then
    # 从 cubemx_utils 的 .mk 文件中提取编译标志
    export RET_CFLAGS=$(find $CUBEMX_UTILS_DIR -type f -name *.mk -exec cat {} \; | python3 $MICROROS_STATIC_LIBRARY_PATH/extract_flags.py)
    RET_CODE=$?

    if [ "$RET_CODE" = "0" ]; then
      echo "Found CFLAGS:"
      echo "-------------"
      echo "$RET_CFLAGS"
      echo "-------------"
    else
      echo "Error retrieving croscompiler flags"
      exit 1
    fi
  fi

  export TOOLCHAIN_PREFIX=/usr/bin/arm-none-eabi-
  set +u
  source "/opt/ros/$ROS_DISTRO/setup.bash"
  set -u

  info "  rosdep install..."
  rosdep install --from-paths src --ignore-src -y

  info "  colcon build..."
  colcon build --cmake-args -Wno-dev

  set +u
  source "$MICROROS_WS/install/setup.bash"
  set -u

  ok "micro-ROS 工作空间编译完成"
  echo ""

  # ==================== Step 4: 创建 firmware 工作空间 ====================
  info "Step 4: 创建 firmware 工作空间..."

  cd "$MICROROS_WS"

  # 加载环境（这里保险起见再加载一次）
  set +u
  source "/opt/ros/$ROS_DISTRO/setup.bash"
  source "$MICROROS_WS/install/setup.bash"
  set -u

  ros2 run micro_ros_setup create_firmware_ws.sh generate_lib

  # 链接自定义消息包到 extra_packages
  info "  链接自定义消息包到 extra_packages ..."
  if [ ! -d "$USER_CUSTOM_PACKAGES_DIR" ]; then
    mkdir -p "$USER_CUSTOM_PACKAGES_DIR"
    ok "  已创建 $USER_CUSTOM_PACKAGES_DIR"
  fi
  ln -sf "$CUSTOM_MSG_SRC" "$USER_CUSTOM_PACKAGES_DIR/$CUSTOM_MSG_PKG"
  ok "  $CUSTOM_MSG_PKG -> $USER_CUSTOM_PACKAGES_DIR/$CUSTOM_MSG_PKG"

  if ! command -v arm-none-eabi-gcc &>/dev/null; then
    warn "  gcc-arm-none-eabi 未安装，正在安装..."
    sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi
  fi
  if ! command -v make &>/dev/null; then
    warn "  make 未安装，正在安装..."
    sudo apt-get install -y make
  fi
  echo ""

  # ==================== Step 5: 编译 firmware ====================
  info "Step 5: 编译 firmware ..."

  cd "$MICROROS_WS"

  info "  configure_firmware.sh ping_pong --transport serial ..."
  ros2 run micro_ros_setup configure_firmware.sh ping_pong --transport serial

  export TOOLCHAIN_PREFIX=/usr/bin/arm-none-eabi-

  if [ -n "${MICROROS_USE_EMBEDDEDRTPS+x}" ]; then
    ros2 run micro_ros_setup build_firmware.sh \
      "$MICROROS_STATIC_LIBRARY_PATH/toolchain.cmake" \
      "$MICROROS_STATIC_LIBRARY_PATH/colcon-embeddedrtps.meta"
  else
    ros2 run micro_ros_setup build_firmware.sh \
      "$MICROROS_STATIC_LIBRARY_PATH/toolchain.cmake" \
      "$MICROROS_STATIC_LIBRARY_PATH/colcon.meta"
  fi
  echo ""
fi

# ==================== Step 6: 整理编译产物 ====================

info "Step 6: 整理编译产物..."
# 清理误编入的 .c 源文件
find "$FW_DIR/build/include/" -name "*.c" -delete

# 重建 libmicroros 输出目录
rm -rf "$LIB_DIR"
mkdir -p "$LIB_DIR/include"
cp -R $FW_DIR/build/include/* "$LIB_DIR/include/"
cp -R "$FW_DIR/build/libmicroros.a" "$LIB_DIR/libmicroros.a"

# 修复 include 路径（消除嵌套的 包名/包名/ 结构）
INCLUDE_ROS2_PACKAGES=$(colcon list | awk '{print $1}')
for var in ${INCLUDE_ROS2_PACKAGES}; do
  if [ -d "$LIB_DIR/include/${var}/${var}" ]; then
    rsync -r "$LIB_DIR/include/${var}/${var}/"* "$LIB_DIR/include/${var}"
    rm -rf "$LIB_DIR/include/${var}/${var}"
  fi
done

# 生成 available_ros2_types
find "$FW_DIR/mcu_ws/ros2" \( -name "*.srv" -o -name "*.msg" -o -name "*.action" \) | awk -F"/" '{print $(NF-2)"/"$NF}' > "$LIB_DIR/available_ros2_types"
find "$FW_DIR/mcu_ws/extra_packages" \( -name "*.srv" -o -name "*.msg" -o -name "*.action" \) | awk -F"/" '{print $(NF-2)"/"$NF}' >> "$LIB_DIR/available_ros2_types"

# 生成 built_packages
echo "" >"$LIB_DIR/built_packages"
cd "$FW_DIR"
echo "" > "$LIB_DIR/built_packages"
while IFS= read -r f; do
  pushd "$f/.." >/dev/null
  echo "$(git config --get remote.origin.url) $(git rev-parse HEAD)" \
    >>"$LIB_DIR/built_packages"
  popd >/dev/null
done < <(find "$MICROROS_STATIC_LIBRARY_PATH" -name .git -type d)

cd "$MICROROS_WS"

# 修复权限
chmod -R u+rwX "$LIB_DIR/"

ok "静态库编译完成"
ok "  libmicroros.a 大小: $(du -sh "$LIB_DIR/libmicroros.a" | cut -f1)"
ok "  头文件数量: $(find "$LIB_DIR/include" -name "*.h" | wc -l)"

# ==================== Step 7: 部署到 STM32 工程 ====================

info "Step 7: 部署到 STM32 工程..."

mkdir -p "$DEST/include"
mkdir -p "$DEST/lib"

info "  拷贝 libmicroros.a ..."
cp "$LIB_DIR/libmicroros.a" "$DEST/lib/"
ok "  libmicroros.a -> $DEST/lib/"

info "  拷贝头文件..."
cp -r "$LIB_DIR/include/"* "$DEST/include/"
HEADER_COUNT=$(find "$DEST/include" -name "*.h" | wc -l)
ok "  头文件拷贝完成 (共 $HEADER_COUNT 个 .h 文件)"

# 拷贝额外源文件
EXTRA_SOURCES=(
  "custom_memory_manager.c"
  "microros_allocators.c"
  "microros_time.c"
  "microros_transports/dma_transport.c"
)

EXTRA_SRC_DIR="$CUBEMX_UTILS_DIR/extra_sources"
mkdir -p "$DEST/src/microros_transports"

info "  拷贝 extra_sources..."
for src_file in "${EXTRA_SOURCES[@]}"; do
  src_path="$EXTRA_SRC_DIR/$src_file"
  dst_path="$DEST/src/$src_file"
  dst_dir="$(dirname "$dst_path")"
  mkdir -p "$dst_dir"
  if [ -f "$src_path" ]; then
    cp "$src_path" "$dst_path"
    ok "  $src_file -> $DEST/src/$src_file"
  else
    warn "  $src_file 不存在: $src_path，跳过"
  fi
done

echo ""

# ==================== Step 8: 验证 ====================

info "Step 8: 验证部署结果..."
echo ""

CHECK_LIST=(
  "$DEST/lib/libmicroros.a|静态库"
  "$DEST/include/rclc/rclc.h|rclc 头文件"
  "$DEST/include/rmw_microxrcedds_c/config.h|RMW 配置"
  "$DEST/include/sensor_msgs/msg/imu.h|sensor_msgs/Imu"
  "$DEST/include/sensor_msgs/msg/battery_state.h|sensor_msgs/BatteryState"
)

for src_file in "${EXTRA_SOURCES[@]}"; do
  CHECK_LIST+=("$DEST/src/$src_file|源文件: $src_file")
done

if [ -d "$DEST/include/$CUSTOM_MSG_PKG/msg" ]; then
  for msg_h in "$DEST/include/$CUSTOM_MSG_PKG/msg/"*.h; do
    [ -f "$msg_h" ] || continue
    CHECK_LIST+=("$msg_h|自定义: $(basename "$msg_h" .h)")
  done
fi

ALL_PASS=true
for item in "${CHECK_LIST[@]}"; do
  filepath="${item%%|*}"
  desc="${item##*|}"
  if [ -f "$filepath" ]; then
    ok "  ✓ $desc"
  else
    warn "  ✗ $desc  ($filepath)"
    ALL_PASS=false
  fi
done

echo ""

if $ALL_PASS; then
  info "=========================================="
  ok " 全部完成！"
  info "=========================================="
else
  warn "=========================================="
  warn " 部分文件缺失，请检查上方警告"
  warn "=========================================="
fi

echo ""
info "STM32CubeIDE 配置提醒："
echo ""
echo "  ① Include paths:        $STM32_MICROROS_DIR/include"
echo "  ② Library search path:  $STM32_MICROROS_DIR/lib"
echo "  ③ Link library:         microros"
echo "  ④ 编译源文件添加:"
echo "     $STM32_MICROROS_DIR/src/custom_memory_manager.c"
echo "     $STM32_MICROROS_DIR/src/microros_allocators.c"
echo "     $STM32_MICROROS_DIR/src/microros_time.c"
echo "     $STM32_MICROROS_DIR/src/microros_transports/dma_transport.c"
