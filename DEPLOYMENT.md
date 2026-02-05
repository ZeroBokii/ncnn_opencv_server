# NCNN OpenCV Server 部署指南

## 系统要求

- **操作系统**: Ubuntu 20.04+ / Debian 11+ 或其他主流 Linux 发行版
- **CPU**: x86_64 或 aarch64 架构
- **内存**: 至少 4GB RAM
- **编译器**: GCC >= 9.0 (支持 C++20)
- **CMake**: >= 3.10

---

## 依赖安装

系统依赖是架构无关的，x86_64 和 aarch64 编译都需要安装以下依赖。

### 更新系统包管理器

```bash
sudo apt-get update
```

### 安装编译工具

```bash
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git
```

### 安装运行时依赖

```bash
sudo apt-get install -y \
    nlohmann-json3-dev \
    libomp-dev \
    libboost-dev
```

### 交叉编译额外依赖（仅 aarch64 交叉编译需要）

```bash
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu
```

> **说明**: 核心依赖库（NCNN、OpenCV、spdlog、inotify-cpp）已预编译并存放在 `lib/` 目录下，无需系统安装。

---

## 项目结构

```
ncnn_opencv_server/
├── lib/
│   ├── amd/                    # x86_64 架构预编译库
│   │   ├── install_ncnn/
│   │   ├── install_opencv/
│   │   ├── install_spdlog/
│   │   ├── install_inotify/
│   │   └── httplib.h
│   └── arm/                    # aarch64 架构预编译库
│       ├── install_ncnn/
│       ├── install_opencv/
│       ├── install_spdlog/
│       ├── install_inotify/
│       └── httplib.h
├── build.sh                    # 统一构建脚本
├── scripts/
│   └── install.sh              # 服务部署脚本
└── workspace/
    ├── ncnn_opencv_server      # 编译输出的可执行文件
    ├── configs/
    └── models/
```

---

## 编译

使用统一构建脚本 `build.sh` 进行编译：

### x86_64 本地编译（开发调试）

```bash
./build.sh x86
```

### aarch64 交叉编译（嵌入式部署）

```bash
./build.sh arm
```

### 清理后重新编译

```bash
./build.sh x86 clean
./build.sh arm clean
```

### 清理所有构建目录

```bash
./build.sh clean
```

编译成功后，可执行文件生成在 `workspace/ncnn_opencv_server`。

---

## 部署为系统服务

编译完成后，使用 `install.sh` 将程序注册为 systemd 服务：

```bash
# 在项目根目录执行
sudo ./install.sh
```

该脚本会：
1. 检查可执行文件是否存在
2. 创建 systemd 服务文件
3. 启用开机自启动
4. 启动服务

---

## 服务管理

```bash
# 查看状态
sudo systemctl status ncnn-server

# 查看日志
sudo journalctl -u ncnn-server -f

# 重启服务
sudo systemctl restart ncnn-server

# 停止服务
sudo systemctl stop ncnn-server

# 禁用开机自启
sudo systemctl disable ncnn-server

# 删除服务
sudo rm /etc/systemd/system/ncnn-server.service
sudo systemctl daemon-reload
```

---

## 嵌入式设备部署

### 1. 在开发机上交叉编译

```bash
./build.sh arm
```

### 2. 打包部署文件

```bash
# 在项目根目录执行，使用 --transform 添加顶层目录
tar -czvf ncnn_server_arm.tar.gz \
    --transform 's,^,ncnn_opencv_server/,' \
    --transform 's,scripts/,,' \
    workspace/ncnn_opencv_server \
    workspace/configs \
    workspace/models \
    lib/arm \
    scripts/install.sh
```

### 3. 传输到目标设备

```bash
scp ncnn_server_arm.tar.gz ideaformer@192.168.1.94:/home/ideaformer/
```

### 4. 在目标设备上部署

```bash
tar -xzvf ncnn_server_arm.tar.gz
cd ncnn_opencv_server

# 部署为服务
sudo ./install.sh
```

---

## 注意事项

1. 确保 `lib/amd/` 和 `lib/arm/` 下的预编译库与目标系统兼容
2. 如果遇到 GLIBC 版本问题，需要在目标系统上重新编译依赖库
3. 动态链接的库需要在运行时设置 `LD_LIBRARY_PATH` 或复制到系统库目录
