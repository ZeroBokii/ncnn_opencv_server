#!/bin/bash

# NCNN OpenCV Server - systemd 服务部署脚本
# 用途：将已编译的程序注册为系统服务
# 前置条件：已执行 ./build.sh 完成编译

SERVICE_NAME="ncnn-server"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="${CURRENT_DIR}/workspace"
EXECUTABLE="${WORKSPACE_DIR}/ncnn_opencv_server"

echo "======================================"
echo "  NCNN OpenCV Server - 服务部署"
echo "======================================"
echo "项目目录: $CURRENT_DIR"
echo "工作目录: $WORKSPACE_DIR"
echo "可执行文件: $EXECUTABLE"
echo ""

# 检查 root 权限
if [ "$EUID" -ne 0 ]; then 
    echo "❌ 错误: 需要 root 权限来创建系统服务"
    echo "请使用: sudo ./install.sh"
    exit 1
fi

echo "[1/6] 检查可执行文件..."
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ 错误: 可执行文件不存在: $EXECUTABLE"
    echo ""
    echo "请先执行编译:"
    echo "  ./build.sh x86    # 本地 x86_64 编译"
    echo "  ./build.sh arm    # 交叉编译 aarch64"
    exit 1
fi
echo "✅ 可执行文件存在"
echo ""

echo "[2/6] 检查配置文件..."
CONFIG_FILE="${WORKSPACE_DIR}/configs/config.json"
MODEL_FILE="${WORKSPACE_DIR}/models/model.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "⚠️  警告: 相机配置文件不存在: $CONFIG_FILE"
fi

if [ ! -f "$MODEL_FILE" ]; then
    echo "⚠️  警告: 模型配置文件不存在: $MODEL_FILE"
fi
echo "✅ 配置检查完成"
echo ""

systemctl stop $SERVICE_NAME 2>/dev/null

echo "[3/8] 安装系统依赖..."
apt-get update -qq
apt-get install -y libmosquitto-dev > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ libmosquitto-dev 已安装"
else
    echo "⚠️  警告: libmosquitto-dev 安装失败，MQTT 功能可能不可用"
fi
echo ""

LIB_DIR=""
if [ -d "${CURRENT_DIR}/lib/arm" ]; then
    LIB_DIR="${CURRENT_DIR}/lib/arm"
elif [ -d "${CURRENT_DIR}/lib/amd" ]; then
    LIB_DIR="${CURRENT_DIR}/lib/amd"
fi

echo "[4/8] 安装动态库到系统..."
if [ -n "$LIB_DIR" ]; then
    TARGET_DIRS=("install_opencv" "install_inotify" "install_spdlog")
    
    for dir in "${TARGET_DIRS[@]}"; do
        if [ -d "${LIB_DIR}/$dir/lib" ]; then
            echo "正在同步 $dir..."
            sudo cp -rdf ${LIB_DIR}/$dir/lib/lib* /usr/local/lib/ 2>/dev/null
        fi
    done

    echo "正在创建 OpenCV 符号链接..."
    cd /usr/local/lib
    for lib in libopencv_core libopencv_imgproc libopencv_imgcodecs; do
        if [ -f "${lib}.so.4.10.0" ]; then
            sudo rm -f ${lib}.so.410 ${lib}.so 2>/dev/null
            sudo ln -sf ${lib}.so.4.10.0 ${lib}.so.410
            sudo ln -sf ${lib}.so.410 ${lib}.so
            echo "  ✓ ${lib} 链接已创建"
        fi
    done
    cd - > /dev/null

    sudo ldconfig
    echo "✅ 动态库已安装并成功刷新缓存"
else
    echo "⚠️ 未找到库目录，跳过动态库安装"
fi

echo "[5/8] 创建 systemd 服务文件..."
cat > "$SERVICE_FILE" << EOF
[Unit]
Description=NCNN OpenCV Inference Server
After=network.target

[Service]
Type=simple
WorkingDirectory=$WORKSPACE_DIR
ExecStart=$EXECUTABLE
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

if [ $? -eq 0 ]; then
    echo "✅ 服务文件已创建: $SERVICE_FILE"
else
    echo "❌ 创建服务文件失败"
    exit 1
fi
echo ""

# 重载 systemd 配置并启用服务
echo "[6/8] 配置 systemd 服务..."
chmod +x "$EXECUTABLE"
systemctl daemon-reload
systemctl enable $SERVICE_NAME
echo "✅ 服务已启用开机自启动"
echo ""

# 启动服务
echo "[7/8] 启动服务..."
systemctl start $SERVICE_NAME

if [ $? -eq 0 ]; then
    echo "✅ 服务启动成功"
else
    echo "❌ 服务启动失败"
    systemctl status $SERVICE_NAME --no-pager -l
    exit 1
fi

echo ""
echo "======================================"
echo "部署完成! 🎉"
echo "======================================"
echo ""
echo "服务管理命令:"
echo "  查看状态: sudo systemctl status $SERVICE_NAME"
echo "  查看日志: sudo journalctl -u $SERVICE_NAME -f"
echo "  重启服务: sudo systemctl restart $SERVICE_NAME"
echo "  停止服务: sudo systemctl stop $SERVICE_NAME"
echo "  禁用服务: sudo systemctl disable $SERVICE_NAME"
echo ""
echo "配置文件:"
echo "  相机配置: $WORKSPACE_DIR/configs/config.json"
echo "  模型配置: $WORKSPACE_DIR/models/model.json"
echo "======================================"
