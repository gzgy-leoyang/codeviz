#!/bin/bash
# 一键构建脚本 - 调用 cmake 和 make 完成编译
set -e

BUILD_TYPE="Debug"

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        *)
            echo "用法: $0 [--release]"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=== codeviz 构建 ==="
echo "构建类型: ${BUILD_TYPE}"
echo "构建目录: ${BUILD_DIR}"

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${SCRIPT_DIR}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "=== 构建完成 ==="
echo "可执行文件: ${BUILD_DIR}/codeviz"
