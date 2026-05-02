#!/bin/bash
# codeviz 运行环境检测脚本
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

# 检测包管理器
detect_pm() {
    if command -v apt-get &>/dev/null; then echo "apt"
    elif command -v dnf &>/dev/null; then echo "dnf"
    elif command -v yum &>/dev/null; then echo "yum"
    elif command -v pacman &>/dev/null; then echo "pacman"
    elif command -v zypper &>/dev/null; then echo "zypper"
    else echo "unknown"
    fi
}

PM=$(detect_pm)

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  codeviz 运行环境检测${NC}"
echo -e "${CYAN}  包管理器: $PM${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# 所有待检测的依赖项
# 格式: "类型|命令名或包名|建议安装命令"
# 类型: core=核心(必须), opt=可选
CHECKS=(
    "core|cmake|cmake"
    "core|g++|g++"
    "core|make|build-essential"
    "opt|python3|python3"
    "opt|git|git"
    "opt|clang++|clang++"
    "opt|libclang-dev|libclang-dev"
    "opt|node|nodejs"
    "opt|npm|npm"
    "opt|graphviz|graphviz"
)

missing_core=""
missing_opt=""
pass=0
fail=0

for entry in "${CHECKS[@]}"; do
    IFS='|' read -r type name install_hint <<< "$entry"

    if [ "$type" = "core" ]; then
        # 核心依赖：用 command -v 检测
        if command -v "$name" &>/dev/null; then
            echo -e "  ${GREEN}✔${NC} $name ($(command -v "$name"))"
            ((++pass))
        else
            echo -e "  ${RED}✘${NC} $name — 未安装"
            missing_core+="$install_hint "
            ((++fail))
        fi
    else
        # 可选依赖：command -v 或 dpkg/rpm 检测
        found=false
        if command -v "$name" &>/dev/null; then
            echo -e "  ${GREEN}✔${NC} $name ($(command -v "$name"))"
            found=true
        elif dpkg -s "$name" &>/dev/null 2>&1 || rpm -q "$name" &>/dev/null 2>&1; then
            echo -e "  ${GREEN}✔${NC} $name"
            found=true
        fi

        if $found; then
            ((++pass))
        else
            echo -e "  ${RED}✘${NC} $name — 未安装"
            missing_opt+="$install_hint "
            ((++fail))
        fi
    fi
done

echo ""
echo -e "${YELLOW}======= 检测结果 =======${NC}"
echo -e "  通过: ${GREEN}$pass${NC}  缺失: ${RED}$fail${NC}"
echo ""

# 汇总所有缺失
missing_all="$missing_core$missing_opt"
missing_all="${missing_all%" "}"  # 去尾空格

if [ -z "$missing_all" ]; then
    echo -e "${GREEN}所有依赖已满足。${NC}"
    echo ""
    echo -e "执行以下命令分析项目:"
    echo -e "  ${CYAN}./build/output/codeviz -p /path/to/project -o report.html${NC}"
    echo ""
    exit 0
fi

echo -e "${YELLOW}以下依赖需要安装:${NC}"
echo ""
for dep in $missing_all; do
    echo "  - $dep"
done
echo ""

if [ ! -t 0 ]; then
    echo "非交互式终端，跳过安装。请手动安装缺失依赖。"
    exit 1
fi

echo -e "${YELLOW}是否执行安装?${NC}"
echo -n "输入 y 确认安装 (y/N): "
read -r CONFIRM

if [ "$CONFIRM" != "y" ] && [ "$CONFIRM" != "Y" ]; then
    echo "跳过安装。请手动安装缺失依赖后重试。"
    exit 1
fi

echo ""
echo -e "${CYAN}开始安装缺失依赖...${NC}"

case $PM in
    apt)
        sudo apt-get update
        # shellcheck disable=SC2086
        sudo apt-get install -y $missing_all
        ;;
    dnf)
        # shellcheck disable=SC2086
        sudo dnf install -y $missing_all
        ;;
    pacman)
        # shellcheck disable=SC2086
        sudo pacman -S --noconfirm $missing_all
        ;;
    *)
        echo -e "${RED}不支持的包管理器 ($PM)，请手动安装: $missing_all${NC}"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}依赖安装完成。${NC}"
echo ""
echo -e "执行以下命令分析项目:"
echo -e "  ${CYAN}./build/output/codeviz -p /path/to/project -o report.html${NC}"
echo ""
