#!/bin/bash
# 统一构建脚本 - 支持 x86_64 和 aarch64 架构
# 用法:
#   ./build.sh          # 默认编译 x86_64 (本地调试)
#   ./build.sh x86      # 编译 x86_64
#   ./build.sh arm      # 交叉编译 aarch64
#   ./build.sh arm clean # 清理后重新编译 aarch64
#   ./build.sh x86 clean # 清理后重新编译 x86_64

set -e

ARCH="${1:-x86}"  # 默认 x86
CLEAN="${2:-}"

case "$ARCH" in
    x86|x86_64|amd|amd64)
        ARCH_NAME="x86_64"
        BUILD_DIR="build_x86"
        LIB_ARCH="amd"
        TOOLCHAIN_FILE=""
        echo "=== NCNN OpenCV Server - x86_64 Native Build ==="
        ;;
    arm|arm64|aarch64)
        ARCH_NAME="aarch64"
        BUILD_DIR="build_aarch64"
        LIB_ARCH="arm"
        TOOLCHAIN_FILE="cmake/toolchain-aarch64.cmake"
        echo "=== NCNN OpenCV Server - aarch64 Cross Compile ==="
        ;;
    clean)
        echo "Cleaning all build directories..."
        rm -rf build_x86 build_aarch64
        echo "Done."
        exit 0
        ;;
    *)
        echo "Usage: $0 [x86|arm] [clean]"
        echo "  x86  - Build for x86_64 (native, for local debugging)"
        echo "  arm  - Cross-compile for aarch64 (for embedded deployment)"
        echo "  clean - Clean all build directories"
        exit 1
        ;;
esac

echo "Architecture: ${ARCH_NAME}"
echo "Build directory: ${BUILD_DIR}"
echo "Library path: lib/${LIB_ARCH}"

if [ "$CLEAN" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf ${BUILD_DIR}
fi

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

echo "Configuring CMake..."
if [ -n "$TOOLCHAIN_FILE" ]; then
    # 交叉编译
    cmake \
        -DCMAKE_TOOLCHAIN_FILE=../${TOOLCHAIN_FILE} \
        -DCMAKE_BUILD_TYPE=Release \
        -DLIB_ARCH=${LIB_ARCH} \
        ..
else
    # 本地编译
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DLIB_ARCH=${LIB_ARCH} \
        ..
fi

echo "Building..."
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Architecture: ${ARCH_NAME}"
echo "Output: workspace/ncnn_opencv_server"
