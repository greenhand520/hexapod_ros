#!/bin/bash
# ============================================================
#  generate_and_deploy_microros.sh [-c]
#
#  -c  清理 firmware
#
#  在 ws_hexapod_ros/ 下运行
# ============================================================

set -euo pipefail

# ==================== 参数解析 ====================

CLEAN_BUILD=false

while getopts "c" opt; do
    case $opt in
    c) CLEAN_BUILD=true ;;
    *) echo "用法: $0 [-c] "; exit 1 ;;
    esac
done

# ==================== 用户配置区 ====================

ROS_DISTRO="${ROS_DISTRO:-humble}"
SETUP_BRANCH="${SETUP_BRANCH:-$ROS_DISTRO}"
CUBEMX_UTILS_REPO="https://github.com/micro-ROS/micro_ros_stm32cubemx_utils.git"
CUBEMX_UTILS_BRANCH="${CUBEMX_UTILS_BRANCH:-$ROS_DISTRO}"
MICROROS_DIR="Middlewares/micro_ros"

# 自定义消息包名（位于 ws_hexapod_ros/src/ 下）
CUSTOM_MSG_PKG="hexapod_sensor_interface"

# ==================== 路径推导 ====================

SCRIPT_DIR="$(pwd)"
PARENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

MICROROS_WS="$PARENT_DIR/microros_ws"
MICROROS_WS_SRC="$MICROROS_WS/src"
SETUP_DIR="$MICROROS_WS_SRC/micro_ros_setup"
CUBEMX_UTILS_DIR="$PARENT_DIR/micro_ros_stm32cubemx_utils"
STM32_PROJECT_DIR="$PARENT_DIR/hexapod_stm32f405"

FW_DIR="$MICROROS_WS/firmware"
FW_CUBEMX="$FW_DIR/micro_ros_stm32cubemx_utils"
LIB_DIR="$FW_DIR/freertos_apps/microros_olimex_e407_extensions/build"
# firmware 中的 ROS2 包目录（create_firmware_ws.sh 生成后存在）
FW_ROS2_DIR="$FW_DIR/mcu_ws/ros2"

# 自定义消息包源码路径
CUSTOM_MSG_SRC="$SCRIPT_DIR/src/$CUSTOM_MSG_PKG"

DEST="$STM32_PROJECT_DIR/$MICROROS_DIR"

# ==================== 颜色输出 ====================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }

# ==================== 环境检查 ====================

if $CLEAN_BUILD; then
    CLEAN_DESC="清理重建"
else
    CLEAN_DESC="增量构建"
fi

info "=========================================="
info " micro-ROS 静态库 生成 & 部署脚本"
info " 构建模式: $CLEAN_DESC"
info "=========================================="
echo ""
info "脚本运行目录       = $SCRIPT_DIR"
info "micro-ROS 工作空间 = $MICROROS_WS"
info "micro_ros_setup    = $SETUP_DIR"
info "libmicroros.a      = $LIB_DIR"
info "cubemx_utils       = $CUBEMX_UTILS_DIR"
info "自定义消息包       = $CUSTOM_MSG_SRC"
info "STM32 工程         = $STM32_PROJECT_DIR"
info "部署目标           = $DEST"
echo ""

[ -f "/opt/ros/$ROS_DISTRO/setup.bash" ] || fail "ROS2 $ROS_DISTRO 未安装"
[ -d "$SCRIPT_DIR/src" ] || fail "src 目录不存在，请确认在 ws_hexapod_ros/ 下运行"
[ -d "$CUSTOM_MSG_SRC" ] || fail "自定义消息包不存在: $CUSTOM_MSG_SRC"
[ -d "$STM32_PROJECT_DIR" ] || fail "STM32 工程目录不存在: $STM32_PROJECT_DIR"

ok "环境检查通过"
echo ""

# ==================== Step 1: 拉取 micro_ros_setup ====================

info "Step 1: 拉取 micro_ros_setup 到 microros_ws/src/..."

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

# ==================== Step 2: 拉取 micro_ros_stm32cubemx_utils ====================

info "Step 2: 拉取 micro_ros_stm32cubemx_utils..."

if [ -d "$CUBEMX_UTILS_DIR" ]; then
    warn "  $CUBEMX_UTILS_DIR 已存在，跳过克隆"
else
    git clone -b "$CUBEMX_UTILS_BRANCH" \
      "$CUBEMX_UTILS_REPO" \
      "$CUBEMX_UTILS_DIR"
    ok "  micro_ros_stm32cubemx_utils 克隆完成"
fi
echo ""

# ==================== Step 3: 编译 microros_ws ====================

info "Step 3: 编译 micro-ROS 工作空间..."

cd "$MICROROS_WS"

set +u
source "/opt/ros/$ROS_DISTRO/setup.bash"
set -u

info "  rosdep install..."
rosdep install --from-paths src --ignore-src -y

info "  colcon build..."
colcon build

set +u
source "$MICROROS_WS/install/setup.bash"
set -u

ok "micro-ROS 工作空间编译完成"
echo ""

# ==================== Step 4: 生成静态库 ====================

info "Step 4: 生成 micro-ROS 静态库..."

cd "$MICROROS_WS"

set +u
source "/opt/ros/$ROS_DISTRO/setup.bash"
source "$MICROROS_WS/install/setup.bash"
set -u

# ---- 清理逻辑 ----
if $CLEAN_BUILD; then
    if [ -d "$FW_DIR" ]; then
        warn "  -c 指定，清理整个 firmware 目录..."
        rm -rf "$FW_DIR"
    fi
    SKIP_BUILD=false
else
    if [ -f "$LIB_DIR/libmicroros.a" ]; then
        warn "  firmware 已有静态库，跳过构建（使用 -c 强制重建）"
        SKIP_BUILD=true
    else
        info "  firmware 中无静态库，清理整个 firmware 目录"
        rm -rf "$FW_DIR"
        SKIP_BUILD=false
    fi
fi

if ! ${SKIP_BUILD:-false}; then

    # 4.1 生成固件工作空间， ref:https://micro.vulcanexus.org/docs/tutorials/core/first_application_rtos/freertos/
    info "  create_firmware_ws.sh freertos olimex-stm32-e407 ..."
    ros2 run micro_ros_setup create_firmware_ws.sh freertos olimex-stm32-e407

    # 4.2 链接自定义消息包到 firmware/mcu_ws/ros2/
    info "  链接自定义消息包到 firmware/mcu_ws/ros2/ ..."
    if [ -d "$FW_ROS2_DIR" ]; then
        ln -sf "$CUSTOM_MSG_SRC" "$FW_ROS2_DIR/$CUSTOM_MSG_PKG"
        ok "  $CUSTOM_MSG_PKG -> $FW_ROS2_DIR/$CUSTOM_MSG_PKG"
    else
        fail "  $FW_ROS2_DIR 不存在，create_firmware_ws.sh 可能执行失败"
    fi

    # 4.3 检查 make
    if ! command -v make &>/dev/null; then
        warn "  make 未安装，正在安装..."
        sudo apt-get update && sudo apt-get install -y make
    fi

    # 4.4 配置固件
    info "  configure_firmware.sh ping_pong --transport serial ..."
    ros2 run micro_ros_setup configure_firmware.sh ping_pong --transport serial
fi

# 4.5 编译静态库
info "  build_firmware.sh ..."
ros2 run micro_ros_setup build_firmware.sh -f

# 4.6 验证
[ -f "$LIB_DIR/libmicroros.a" ] || fail "libmicroros.a 未生成！"
#[ -d "$LIB_DIR/libmicroros/include" ] || fail "include 目录未生成！"

LIB_SIZE=$(du -sh "$LIB_DIR/libmicroros.a" | cut -f1)
ok "静态库生成成功 (大小: $LIB_SIZE)"

echo ""

# ==================== Step 5: 部署到 STM32 工程 ====================

info "Step 5: 部署到 STM32 工程..."

mkdir -p "$DEST/include"
mkdir -p "$DEST/lib"
mkdir -p "$DEST/src/microros_transports"

info "  拷贝 libmicroros.a ..."
cp "$LIB_DIR/libmicroros.a" "$DEST/lib/"
ok "  libmicroros.a -> $DEST/lib/"

info "  拷贝头文件..."
cp -r "$LIB_DIR/libmicroros/include/"* "$DEST/include/"
HEADER_COUNT=$(find "$DEST/include" -name "*.h" | wc -l)
ok "  头文件拷贝完成 (共 $HEADER_COUNT 个 .h 文件)"

if [ -d "$FW_CUBEMX/microros_transports" ]; then
    info "  拷贝传输层实现..."
    cp -r "$FW_CUBEMX/microros_transports/"* "$DEST/src/microros_transports/"
    ok "  microros_transports/ 拷贝完成"
else
    warn "  microros_transports 目录不存在，跳过"
fi

for timefile in microros_time.c microros_time.h; do
    if [ -f "$FW_CUBEMX/$timefile" ]; then
        cp "$FW_CUBEMX/$timefile" "$DEST/src/"
        ok "  $timefile -> $DEST/src/"
    else
        warn "  $timefile 不存在，跳过"
    fi
done

echo ""

# ==================== Step 6: 验证 ====================

info "Step 6: 验证部署结果..."
echo ""

CHECK_LIST=(
    "$DEST/lib/libmicroros.a|静态库"
    "$DEST/include/rclc/rclc.h|rclc 头文件"
    "$DEST/include/rmw_microxrcedds_c/config.h|RMW 配置"
    "$DEST/include/sensor_msgs/msg/imu.h|sensor_msgs/Imu"
    "$DEST/include/sensor_msgs/msg/battery_state.h|sensor_msgs/BatteryState"
)

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
echo "  ① Include paths:        $MICROROS_DIR/include"
echo "  ② Library search path:  $MICROROS_DIR/lib"
echo "  ③ Link library:         microros"
echo "  ④ 编译源文件添加:"
echo "     $MICROROS_DIR/src/microros_time.c"
echo "     $MICROROS_DIR/src/microros_transports/<your_transport>.c"
