#!/bin/bash
#
# micro-ROS Agent 一键安装脚本
# 源码编译安装到工作空间，自动配置 .bashrc / .zshrc
# 用法: chmod +x install_microros_agent.sh && ./install_microros_agent.sh
#

set -euo pipefail

# ============================================================
# 颜色输出定义
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${BLUE}[INFO]${NC}    $*"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}    $*"; }
error()   { echo -e "${RED}[ERROR]${NC}   $*"; }

# ============================================================
# 配置变量
# ============================================================
ROS_DISTRO="${ROS_DISTRO:-humble}"
WORKSPACE_DIR="$HOME/microros_ws"
CLONE_BRANCH="${ROS_DISTRO}"
CLONE_URL="https://github.com/micro-ROS/micro_ros_setup.git"
MARKER="# >>> micro-ROS Agent >>>"


echo "============================================"
echo "  micro-ROS Agent 安装脚本"
echo "  ROS2 发行版: ${ROS_DISTRO}"
echo "  工作空间:    ${WORKSPACE_DIR}"
echo "============================================"
echo ""

# ============================================================
# Step 1/6: 检查 ROS2 是否安装
# ============================================================
info "===== Step 1/6: 检查 ROS2 ====="

if [ ! -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]; then
  error "ROS2 ${ROS_DISTRO} 未安装，请先安装 ROS2"
  exit 1
fi
success "ROS2 ${ROS_DISTRO} 已安装"
echo ""

# ============================================================
# Step 2/6: 创建工作空间
# ============================================================
info "===== Step 2/6: 创建工作空间 ====="

mkdir -p "${WORKSPACE_DIR}/src"
success "工作空间已创建: ${WORKSPACE_DIR}"
echo ""

# ============================================================
# Step 3/6: 克隆 micro_ros_setup 仓库
# ============================================================
info "===== Step 3/6: 克隆 micro_ros_setup ====="

TARGET_DIR="${WORKSPACE_DIR}/src/micro_ros_setup"

if [ -d "$TARGET_DIR/.git" ]; then
  local_branch=$(git -C "$TARGET_DIR" branch --show-current 2>/dev/null)
  local_commit=$(git -C "$TARGET_DIR" log --oneline -1 2>/dev/null)
  warn "目标目录已存在，跳过克隆: $TARGET_DIR"
  info "分支: ${local_branch}"
elif [ -d "$TARGET_DIR" ]; then
  warn "目录存在但非有效仓库，清理: $TARGET_DIR"
  rm -rf "$TARGET_DIR"
fi

if [ ! -d "$TARGET_DIR" ]; then
  info "命令: git clone -b ${CLONE_BRANCH} ${CLONE_URL} ${TARGET_DIR}"

  if ! git clone -b "$CLONE_BRANCH" "$CLONE_URL" "$TARGET_DIR" 2>&1; then
    error "git clone 失败"
    rm -rf "$TARGET_DIR" 2>/dev/null
    error "手动尝试: git clone -b ${CLONE_BRANCH} ${CLONE_URL}"
    error "或使用镜像: git clone -b ${CLONE_BRANCH} https://gitee.com/mirrors/micro_ros_setup.git"
    exit 1
  fi

  current_commit=$(git -C "$TARGET_DIR" log --oneline -1)
  success "git clone 成功"
  info "分支: ${CLONE_BRANCH}"
fi
echo ""

# ============================================================
# Step 4/6: 编译 micro_ros_setup
# ============================================================
info "===== Step 4/6: 编译 micro_ros_setup ====="

cd "${WORKSPACE_DIR}"

info "source /opt/ros/${ROS_DISTRO}/setup.bash"
set +u
source "/opt/ros/${ROS_DISTRO}/setup.bash"
set -u

info "执行: colcon build --cmake-args -Wno-dev"
if ! colcon build --cmake-args -Wno-dev 2>&1; then
  error "编译 micro_ros_setup 失败"
  exit 1
fi

if [ ! -f "${WORKSPACE_DIR}/install/setup.bash" ]; then
  error "编译产物验证失败: 未找到 install/setup.bash"
  exit 1
fi

success "micro_ros_setup 编译完成"
echo ""

# ============================================================
# Step 5/6: 创建并编译 micro_ros_agent
# ============================================================
info "===== Step 5/6: 创建并编译 micro_ros_agent ====="

cd "${WORKSPACE_DIR}"

set +u
source "/opt/ros/$ROS_DISTRO/setup.bash"
source "$WORKSPACE_DIR/install/setup.bash"
set -u

info "执行: ros2 run micro_ros_setup create_agent_ws.sh"
if ! ros2 run micro_ros_setup create_agent_ws.sh 2>&1; then
  error "创建 Agent 工作空间失败"
  exit 1
fi
success "Agent 工作空间已创建"


info "执行: ros2 run micro_ros_setup build_agent.sh"
if ! ros2 run micro_ros_setup build_agent.sh 2>&1; then
  error "编译 Agent 失败"
  error "查看日志: ${WORKSPACE_DIR}/log/"
  exit 1
fi
success "Agent 编译完成"
echo ""

# ============================================================
# Step 6/6: 配置 shell 环境 + 验证安装
# ============================================================
info "===== Step 6/6: 配置 shell 环境 ====="

SETUP_BASH="${WORKSPACE_DIR}/install/setup.bash"
SETUP_ZSH="${WORKSPACE_DIR}/install/setup.zsh"

# ---- 配置 .bashrc ----
BASHRC="$HOME/.bashrc"
if [ -f "$BASHRC" ]; then
  if grep -qF "source ${SETUP_BASH}" "$BASHRC"; then
    info ".bashrc 中已存在 source 行，跳过"
  else
    info "向 ${BASHRC} 添加 source 行..."
    cat >> "$BASHRC" << EOF

${MARKER}
if [ -f "${SETUP_BASH}" ]; then
    source ${SETUP_BASH}
fi
# <<< micro-ROS Agent <<<
EOF
    success ".bashrc 配置完成"
  fi
else
  warn "未找到 ${BASHRC}，跳过 bash 配置"
fi

# ---- 配置 .zshrc（仅当 zsh 已安装）----
if command -v zsh &> /dev/null; then
  ZSHRC="$HOME/.zshrc"
  if [ -f "$ZSHRC" ]; then
    if grep -qF "source ${SETUP_ZSH}" "$ZSHRC"; then
      info ".zshrc 中已存在 source 行，跳过"
    else
      info "检测到 zsh，向 ${ZSHRC} 添加 source 行..."
      cat >> "$ZSHRC" << EOF

${MARKER}
if [ -f "${SETUP_ZSH}" ]; then
    source ${SETUP_ZSH}
fi
# <<< micro-ROS Agent <<<
EOF
      success ".zshrc 配置完成"
    fi
  else
    info "检测到 zsh，创建 ${ZSHRC}..."
    cat > "$ZSHRC" << EOF
if [ -f "${SETUP_ZSH}" ]; then
    source ${SETUP_ZSH}
fi
# <<< micro-ROS Agent <<<
EOF
    success ".zshrc 已创建并配置"
  fi
else
  info "未检测到 zsh，跳过 .zshrc 配置"
fi
echo ""

# ---- 验证安装结果 ----
info "验证安装结果..."

if [ -f "$SETUP_BASH" ]; then
  success "setup.bash 存在: ${SETUP_BASH}"
else
  error "setup.bash 不存在: ${SETUP_BASH}"
  exit 1
fi

if [ -f "$SETUP_ZSH" ]; then
  success "setup.zsh 存在: ${SETUP_ZSH}"
else
  warn "setup.zsh 不存在: ${SETUP_ZSH} (zsh 用户可能无法使用)"
fi

agent_bin=$(find "${WORKSPACE_DIR}/install" -name "micro_ros_agent" -type f 2>/dev/null | head -1)
if [ -n "$agent_bin" ]; then
  success "micro_ros_agent 可执行文件: ${agent_bin}"
else
  error "未找到 micro_ros_agent 可执行文件"
  exit 1
fi

echo ""
echo "============================================"
success "micro-ROS Agent 安装完成!"
echo "============================================"
echo ""
info "工作空间位置: ${WORKSPACE_DIR} (请勿删除)"
echo ""
info "请重新打开终端，或执行以下命令立即生效:"
echo ""
echo "  bash 用户: source ~/.bashrc"
echo "  zsh  用户: source ~/.zshrc"
echo ""
info "快速测试:"
echo "  ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0"
echo ""
info "自定义消息类型使用方式:"
echo "  source ~/your_msgs_ws/install/setup.bash"
echo "  ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0"
echo ""
