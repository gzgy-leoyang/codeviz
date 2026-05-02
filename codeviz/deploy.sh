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
# 格式: "命令名或包名"
# 仅包含运行 codeviz 及查看报告所需的可选工具
CHECKS=(
    "python3"
    "git"
    "clang++"
    "libclang-dev"
    "node"
    "npm"
    "graphviz"
    "firefox"
)

missing=""
pass=0

for name in "${CHECKS[@]}"; do
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
        missing+="$name "
    fi
done

echo ""
echo -e "${YELLOW}======= 检测结果 =======${NC}"
echo -e "  通过: ${GREEN}$pass/${#CHECKS[@]}${NC}"

# 检测 codeviz 可执行文件
if [ -f "$SCRIPT_DIR/build/output/codeviz" ]; then
    echo -e "  ${GREEN}✔${NC} codeviz 可执行文件已就绪"
else
    echo -e "  ${YELLOW}ℹ${NC} codeviz 未构建，请执行 ${CYAN}bash build.sh${NC} 编译"
fi
echo ""

missing="${missing%" "}"

if [ -z "$missing" ]; then
    echo -e "${GREEN}所有可选依赖已满足。${NC}"
else
    echo -e "以下可选工具未安装（不影响 codeviz 核心功能，部分报告特性可能受限）:"
    echo ""
    for dep in $missing; do
        echo "  - $dep"
    done
    echo ""

    if [ -t 0 ]; then
        echo -e "${YELLOW}是否安装以上可选工具?${NC}"
        echo -n "输入 y 确认安装 (y/N): "
        read -r CONFIRM
        if [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; then
            echo ""
            echo -e "${CYAN}开始安装...${NC}"
            case $(detect_pm) in
                apt)
                    sudo apt-get update
                    # shellcheck disable=SC2086
                    sudo apt-get install -y $missing
                    ;;
                dnf)
                    # shellcheck disable=SC2086
                    sudo dnf install -y $missing
                    ;;
                pacman)
                    # shellcheck disable=SC2086
                    sudo pacman -S --noconfirm $missing
                    ;;
                *)
                    echo -e "${RED}不支持的包管理器，请手动安装: $missing${NC}"
                    exit 1
                    ;;
            esac
            echo -e "${GREEN}安装完成。${NC}"
        else
            echo "跳过安装。"
        fi
    fi
fi

echo ""
echo -e "执行以下命令分析项目:"
echo -e "  ${CYAN}./build/output/codeviz -p /path/to/project -o report.html${NC}"
echo ""
