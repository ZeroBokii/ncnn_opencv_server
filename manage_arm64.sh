#!/bin/bash

# --- 样式定义 ---
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # 无颜色

# --- 参数处理 ---
# 默认值为 true，并将输入转为小写处理
ACTION=${1:-true}
ACTION=$(echo "$ACTION" | tr '[:upper:]' '[:lower:]')

# --- 核心逻辑 ---

if [ "$ACTION" == "false" ]; then
    echo -e "${YELLOW}[!] 正在准备关闭 arm64 交叉编译环境...${NC}"

    # 1. 强制移除所有 arm64 架构的软件包
    # 使用 --allow-remove-essential 是为了跳过 libc6:arm64 等核心库的保护警告
    echo -e "${RED}步骤 1/3: 正在强制卸载所有 :arm64 软件包...${NC}"
    sudo apt-get purge ".*:arm64" --allow-remove-essential -y

    # 2. 移除架构
    echo -e "${RED}步骤 2/3: 正在从 dpkg 中移除 arm64 架构...${NC}"
    sudo dpkg --remove-architecture arm64

    # 3. 清理缓存并更新
    echo -e "${RED}步骤 3/3: 清理索引并更新软件源...${NC}"
    sudo rm -rf /var/lib/apt/lists/*
    sudo apt-get update

    echo -e "${GREEN}[✔] 完成！arm64 交叉编译环境已彻底关闭。${NC}"

else
    echo -e "${YELLOW}[+] 正在准备开启 arm64 交叉编译环境...${NC}"

    # 1. 添加架构
    echo -e "${GREEN}步骤 1/2: 正在向 dpkg 添加 arm64 架构...${NC}"
    sudo dpkg --add-architecture arm64

    # 2. 清理并更新
    echo -e "${GREEN}步骤 2/3: 正在清理并更新软件源索引 (这可能需要一些时间)...${NC}"
    sudo rm -rf /var/lib/apt/lists/*
    sudo apt-get update

    # 3. 安装 arm64 依赖库
    echo -e "${GREEN}步骤 3/3: 正在安装 arm64 交叉编译依赖库...${NC}"
    sudo apt-get install -y libmosquitto-dev:arm64

    echo -e "${GREEN}[✔] 完成！arm64 交叉编译环境已就绪。${NC}"
fi