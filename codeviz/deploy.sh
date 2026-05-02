#!/bin/bash
# codeviz 环境部署脚本
# 检查构建和运行依赖，引导用户安装缺失组件
# 对应设计文档 2.6 IR_6

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

pass=0
fail=0
warn=0

check_cmd() {
    if command -v "$1" &>/dev/null; then
        echo -e "  ${GREEN}✔${NC} $1 ($(command -v "$1"))"
        return 0
    else
        echo -e "  ${RED}✘${NC} $1 — 未找到"
        return 1
    fi
}

check_pkg() {
    local pkg="$1"
    local hint="$2"
    if dpkg -s "$pkg" &>/dev/null 2>&1 || rpm -q "$pkg" &>/dev/null 2>&1; then
        echo -e "  ${GREEN}✔${NC} $pkg"
        return 0
    else
        echo -e "  ${RED}✘${NC} $pkg — 未安装${hint:+ ($hint)}"
        return 1
    fi
}

detect_pm() {
    if command -v apt-get &>/dev/null; then
        echo "apt"
    elif command -v dnf &>/dev/null; then
        echo "dnf"
    elif command -v yum &>/dev/null; then
        echo "yum"
    elif command -v pacman &>/dev/null; then
        echo "pacman"
    elif command -v zypper &>/dev/null; then
        echo "zypper"
    else
        echo "unknown"
    fi
}

PM=$(detect_pm)
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  codeviz 环境部署脚本${NC}"
echo -e "${CYAN}  包管理器: $PM${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# ============================================================
# 核心构建依赖
# ============================================================
echo -e "${YELLOW}[1/4] 核心构建工具${NC}"
echo ""

if check_cmd cmake; then
    ((++pass))
else
    echo -e "  ${YELLOW}→ 建议: apt install cmake${NC}"
    ((fail++))
fi

if check_cmd g++ || check_cmd clang++; then
    ((++pass))
else
    echo -e "  ${YELLOW}→ 建议: apt install g++${NC}"
    ((fail++))
fi

if check_cmd make; then
    ((++pass))
else
    echo -e "  ${YELLOW}→ 建议: apt install build-essential${NC}"
    ((fail++))
fi

echo ""

# ============================================================
# 开发工具链
# ============================================================
echo -e "${YELLOW}[2/4] 开发工具链（可选）${NC}"
echo ""

if check_cmd python3; then ((++pass)); else ((++warn)); fi
if check_cmd git; then ((++pass)); else ((++warn)); fi
if check_cmd clang++; then ((++pass)); else ((++warn)); fi
if check_pkg "libclang-dev" "apt install libclang-dev"; then ((++pass)); else ((++warn)); fi
if check_cmd node; then ((++pass)); else ((++warn)); fi
if check_cmd npm; then ((++pass)); else ((++warn)); fi

echo ""

# ============================================================
# 可视化工具（可选）
# ============================================================
echo -e "${YELLOW}[3/4] 可视化工具（可选）${NC}"
echo ""

if check_cmd graphviz; then ((++pass)); else ((++warn)); fi

echo ""

# ============================================================
# 汇总 & 安装引导
# ============================================================
echo -e "${YELLOW}[4/4] 汇总${NC}"
echo ""
echo -e "  通过: ${GREEN}$pass${NC}  缺失: ${RED}$fail${NC}  可选: ${YELLOW}$warn${NC}"
echo ""

MISSING_DEPS=""
if ! command -v cmake &>/dev/null; then MISSING_DEPS+="cmake "; fi
if ! command -v g++ &>/dev/null && ! command -v clang++ &>/dev/null; then MISSING_DEPS+="g++ "; fi
if ! command -v make &>/dev/null; then MISSING_DEPS+="build-essential "; fi

if [ $fail -gt 0 ]; then
    if [ -t 0 ]; then
        echo -e "${RED}缺少核心构建依赖，是否尝试自动安装?${NC}"
        echo -n "输入 y 确认安装 (y/N): "
        read -r CONFIRM
    else
        CONFIRM=""
    fi
    if [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; then
        case $PM in
            apt)
                sudo apt-get update
                # shellcheck disable=SC2086
                sudo apt-get install -y $MISSING_DEPS
                ;;
            dnf)
                sudo dnf install -y cmake gcc-c++ make
                ;;
            pacman)
                sudo pacman -S --noconfirm cmake gcc make
                ;;
            *)
                echo -e "${RED}不支持的包管理器，请手动安装: $MISSING_DEPS${NC}"
                exit 1
                ;;
        esac
        echo -e "${GREEN}依赖安装完成，继续构建${NC}"
    else
        echo "跳过安装，尝试直接构建..."
    fi
fi

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  开始构建 codeviz${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

bash "$SCRIPT_DIR/build.sh"

echo ""
echo -e "${GREEN}部署完成！${NC}"
echo -e "执行以下命令分析项目:"
echo -e "  ${CYAN}./build/output/codeviz -p /path/to/project -o report.html${NC}"
