# NCNN OpenCV 智能推理服务器

> 基于 NCNN + OpenCV 的高性能多相机多算法实时推理系统

![Version](https://img.shields.io/badge/version-2.0-blue)
![C++](https://img.shields.io/badge/C%2B%2B-20-green)
![Platform](https://img.shields.io/badge/platform-Linux-orange)
![License](https://img.shields.io/badge/license-MIT-brightgreen)

## 📚 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [系统架构](#系统架构)
- [技术栈](#技术栈)
- [目录结构](#目录结构)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [部署指南](#部署指南)
- [批量推理工具](#批量推理工具)
- [开发指南](#开发指南)
- [常见问题](#常见问题)

---

## 📖 项目简介

**NCNN OpenCV 智能推理服务器** 是一个高性能、模块化的 C++ 推理系统，专为边缘设备和生产环境设计。系统支持：

- 🎯 **多相机并发推理**：同时管理多个图像源，独立配置算法
- 🧠 **多算法协同**：灵活组合不同 AI 算法（检测、分类、分割等）
- 📁 **文件监听驱动**：自动检测新图像并触发推理，无需手动干预
- ⚡ **高性能推理**：基于 NCNN 优化，支持 INT8 量化，OpenMP 多线程
- 🔄 **热加载支持**：运行时动态更新配置和模型
- 📊 **结构化输出**：JSON 格式推理结果，便于集成

**典型应用场景：**
- 工业视觉检测（质量控制、缺陷检测）
- 智能监控系统（多摄像头目标检测）
- 3D 打印监控（实时故障检测，如面条检测）
- 边缘 AI 推理服务

---

## ✨ 核心特性

### 1. 多相机管理架构

- **独立相机配置**：每个相机可配置独立的算法组合
- **并发推理**：多相机并行处理，互不干扰
- **文件监听器**：每个相机可监听不同目录，自动触发推理

### 2. 灵活的算法管理

- **算法工厂模式**：动态创建和管理算法实例
- **共享/独占模式**：
  - **共享算法**：多相机共用同一模型实例（节省内存）
  - **独占算法**：每个相机独立模型实例（最大并发）
- **插件式扩展**：轻松添加新算法类型（检测/分类/分割）

### 3. 高性能推理引擎

- **NCNN 深度优化**：支持 Vulkan/CPU 后端
- **INT8 量化支持**：模型压缩，推理加速
- **OpenMP 并行**：充分利用多核 CPU
- **内存池管理**：减少内存分配开销
- **结果缓存**：避免重复推理

### 4. 智能文件监听

- **基于 inotify**：Linux 内核级文件监控
- **事件去重**：避免重复处理同一文件
- **扩展名过滤**：仅处理指定图像格式
- **异步处理**：监听和推理解耦，不阻塞

### 5. 完善的日志系统

- **分级日志**：DEBUG/INFO/WARN/ERROR 级别
- **延迟文件写入**：仅在出错时创建日志文件
- **彩色控制台输出**：提升可读性
- **自动归档**：按时间戳生成日志文件

### 6. HTTP API 控制接口

- **推理开关控制**：通过 REST API 动态开启/关闭推理功能
- **线程安全**：原子操作保证并发安全
- **即时生效**：无需重启服务

### 7. MQTT 消息通知

- **缺陷检测通知**：检测到缺陷时通过 MQTT 发布消息
- **自动重连**：断线后自动重连（1~10秒指数回退）
- **非阻塞发送**：后台线程处理网络，不影响推理性能

---

## 🏗️ 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                        │
│                          (App.cpp)                              │
└────────────┬────────────────────────────────────────────────────┘
             │
             ├──────────────────┬──────────────────┬──────────────┐
             ▼                  ▼                  ▼              ▼
    ┌────────────────┐ ┌────────────────┐ ┌─────────────┐ ┌──────────┐
    │  MyController  │ │ MultiCamera    │ │ Inference   │ │   File   │
    │  (配置管理)     │ │  Manager       │ │  Engine     │ │ Watcher  │
    │                │ │  (相机管理)     │ │ (推理引擎)   │ │ (监听器)  │
    └────────────────┘ └────────┬───────┘ └──────┬──────┘ └────┬─────┘
                                │                 │              │
                                ▼                 ▼              │
                       ┌─────────────────┐ ┌─────────────┐      │
                       │ CameraInstance  │ │  Algorithm  │      │
                       │   (相机实例)     │ │  Manager    │      │
                       │                 │ │ (算法管理)   │      │
                       └─────────────────┘ └──────┬──────┘      │
                                                   │              │
                                                   ▼              │
                                          ┌─────────────────┐    │
                                          │  Detection      │    │
                                          │  Algorithm      │    │
                                          │                 │    │
                                          └────────┬────────┘    │
                                                   │              │
                                                   ▼              │
                                          ┌─────────────────┐    │
                                          │     NCNN        │    │
                                          │   Inference     │    │
                                          │   (推理核心)     │    │
                                          └─────────────────┘    │
                                                                  │
                                          inotify Events ─────────┘
```

### 核心模块说明

#### 1. **应用层 (App.cpp)**
- 系统初始化和启动
- 日志配置（控制台 + 文件）
- 主事件循环管理
- 推理结果处理回调

#### 2. **配置管理 (MyController)**
- 加载相机配置 (`config.json`)
- 加载模型配置 (`model.json`)
- 配置验证和解析

#### 3. **多相机管理 (MultiCameraManager)**
- 管理多个相机实例
- 相机生命周期控制（启动/停止）
- 文件监听器管理
- 推理回调分发

#### 4. **推理引擎 (InferenceEngine)**
- 相机-算法映射管理
- 共享/独占算法协调
- 推理任务调度
- 结果缓存管理

#### 5. **算法管理 (AlgorithmManager)**
- 算法配置加载
- 算法实例创建（工厂模式）
- 算法类型管理（检测/分类/分割）
- 并发推理控制

#### 6. **检测算法 (DetectionAlgorithm)**
- YOLO 系列算法封装
- 预处理流水线（Preprocessor）
- 后处理流水线（Postprocessor）
- 结果可视化

#### 7. **NCNN 推理 (NcnnInference)**
- NCNN 模型加载
- 前向推理
- Vulkan/CPU 后端切换
- INT8 量化支持

#### 8. **文件监听 (FileWatcher)**
- Linux inotify 封装
- 文件事件过滤（CREATE/MODIFY/CLOSE_WRITE）
- 事件去重机制
- 异步回调通知

---

## 🛠️ 技术栈

| 类别 | 技术 | 版本要求 | 用途 |
|------|------|----------|------|
| **编程语言** | C++ | >= C++20 | 核心开发语言 |
| **编译器** | GCC | >= 9.0 | 支持 C++20 特性 |
| **构建系统** | CMake | >= 3.10 | 项目构建管理 |
| **推理框架** | NCNN | latest | 深度学习推理 |
| **计算机视觉** | OpenCV | >= 4.0 | 图像处理 |
| **日志库** | spdlog | latest | 高性能日志 |
| **JSON 解析** | nlohmann_json | >= 3.0 | 配置和结果序列化 |
| **文件监听** | inotify-cpp | latest | Linux 文件监控 |
| **HTTP 库** | cpp-httplib | latest | REST API 服务 & HTTP 客户端 |
| **MQTT 库** | libmosquitto | latest | MQTT 消息发布 |
| **并行计算** | OpenMP | latest | 多线程加速 |

---

## 📁 目录结构

```
ncnn_opencv_server/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明文档（本文件）
├── DEPLOYMENT.md               # 部署和服务配置指南
├── ARCHITECTURE.md             # 详细架构设计文档
├── Project_Handover.md         # 项目交接文档
├── build.sh                    # 统一构建脚本（支持 x86/arm）
│
├── src/                        # 源代码目录
│   ├── App.cpp                # 主程序入口
│   ├── common/                # 公共模块
│   ├── camera/                # 相机管理模块
│   ├── algorithms/            # 算法管理模块
│   ├── inferEngine/           # 推理引擎模块
│   ├── deploy/                # NCNN 推理封装
│   ├── inotify/               # 文件监听模块
│   ├── api/                   # HTTP API 模块
│   └── tools/                 # 工具类
│
├── scripts/                    # 脚本目录
│   ├── start.sh               # 服务部署脚本
│   ├── batch_infer.py         # 批量推理工具
│   └── manage_arm64.sh        # ARM64 管理脚本
│
├── lib/                        # 预编译依赖库（按架构分类）
│   ├── amd/                    # x86_64 架构
│   │   ├── install_ncnn/
│   │   ├── install_opencv/
│   │   ├── install_spdlog/
│   │   ├── install_inotify/
│   │   └── httplib.h
│   └── arm/                    # aarch64 架构
│       ├── install_ncnn/
│       ├── install_opencv/
│       ├── install_spdlog/
│       ├── install_inotify/
│       └── httplib.h
│
├── build_x86/                  # x86_64 构建目录（自动生成）
├── build_aarch64/              # aarch64 构建目录（自动生成）
│
└── workspace/                  # 运行时工作目录
    ├── ncnn_opencv_server      # 编译后的可执行文件
    ├── configs/
    │   └── config.json         # 相机配置
    ├── models/
    │   ├── model.json          # 模型配置
    │   └── <model_folders>/    # 模型文件目录
    ├── logs/                   # 日志（自动生成）
    └── output_results/         # 推理结果（自动生成）
```

---

## 🚀 快速开始

### 1. 环境准备

确保系统满足以下要求：
- Ubuntu 20.04 / 22.04 LTS
- GCC >= 9.0
- CMake >= 3.10
- 至少 4GB 内存

### 2. 安装依赖

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    nlohmann-json3-dev \
    libomp-dev \
    libboost-dev \
    libmosquitto-dev \
    mosquitto \
    mosquitto-clients
```

> **说明**: 核心依赖库（NCNN、OpenCV、spdlog、inotify-cpp）已预编译在 `lib/` 目录下，无需系统安装。

### 3. 编译项目

```bash
# 进入项目目录
cd ncnn_opencv_server

# x86_64 本地编译（开发调试）
./build.sh x86

# 或 aarch64 交叉编译（嵌入式部署）
./build.sh arm
```

编译成功后，可执行文件位于 `workspace/ncnn_opencv_server`。

### 4. 配置系统

#### 4.1 配置相机和算法映射 (`workspace/configs/config.json`)

```json
{
    "config_mapping": {
        "camera_top": {
            "alg": ["pose"],
            "watcher": "/home/torch/images/pose"
        },
        "camera_left": {
            "alg": ["detection", "classification"],
            "watcher": "/home/user/images/left"
        }
    }
}
```

- `camera_top`：相机 ID（自定义名称）
- `alg`：该相机使用的算法列表
- `watcher`：监听的图像目录路径

#### 4.2 配置模型 (`workspace/models/model.json`)

```json
{
    "models": [
        {
            "type": "Detection",
            "name": "pose",
            "model_param": "./models/yolo11n/int8.param",
            "model_bin": "./models/yolo11n/int8.bin",
            "label_path": "./models/yolo11n/label.txt",
            "preprocess": "yolo11",
            "in_name": "in0",
            "out_name": "out0"
        }
    ]
}
```

- `type`：算法类型（Detection/Classification/Segmentation）
- `name`：算法名称（与 config.json 中的 `alg` 对应）
- `model_param/model_bin`：NCNN 模型文件路径
- `preprocess`：预处理类型（yolo11/yolov8/yolov5）

### 5. 运行服务

```bash
# 方式一：直接运行
cd workspace
./ncnn_opencv_server

# 方式二：部署为系统服务
sudo ./start.sh
```

### 6. 测试推理

在监听目录中放入测试图像：

```bash
# 复制图像到监听目录
cp test_image.jpg /home/torch/images/pose/
```

系统将自动检测图像并执行推理，输出 JSON 格式结果：

```json
{
    "camera_id": "camera_top",
    "algorithm": "pose",
    "result": {
        "time_ms": 45,
        "detections": [
            {
                "box": {"left": 120, "top": 80, "right": 350, "bottom": 450},
                "cls_id": 0,
                "score": 0.95,
                "label": "person"
            }
        ]
    }
}
```

可视化结果将保存到 `workspace/output_results/`。

---

## ⚙️ 配置说明

### 相机配置详解

```json
{
    "config_mapping": {
        "<camera_id>": {
            "alg": ["<algorithm_name_1>", "<algorithm_name_2>"],
            "watcher": "<watch_directory_path>"
        }
    }
}
```

- **camera_id**：相机唯一标识符，自定义命名
- **alg**：该相机使用的算法名称列表，必须在 `model.json` 中定义
- **watcher**：监听的目录路径，新图像将自动触发推理

### 模型配置详解

```json
{
    "models": [
        {
            "type": "<algorithm_type>",
            "name": "<algorithm_name>",
            "model_param": "<ncnn_param_file_path>",
            "model_bin": "<ncnn_bin_file_path>",
            "label_path": "<label_file_path>",
            "preprocess": "<preprocess_type>",
            "in_name": "<input_layer_name>",
            "out_name": "<output_layer_name>"
        }
    ]
}
```

**参数说明：**
- **type**：算法类型，目前支持 `Detection`
- **name**：算法唯一名称，与 `config.json` 中的 `alg` 对应
- **model_param**：NCNN 模型参数文件（.param）
- **model_bin**：NCNN 模型权重文件（.bin）
- **label_path**：类别标签文件（每行一个类别名）
- **preprocess**：预处理类型（yolo11/yolov8/yolov5）
- **in_name**：NCNN 输入层名称（通常为 `in0` 或 `images`）
- **out_name**：NCNN 输出层名称（通常为 `out0` 或 `output`）

### 日志级别配置

在 `App.cpp` 中修改日志级别：

```cpp
// 控制台日志级别
console_sink->set_level(spdlog::level::debug);  // debug/info/warn/err

// 文件日志级别（仅记录错误）
lazy_sink->set_level(spdlog::level::err);

// 全局日志级别
spdlog::set_level(spdlog::level::debug);
```

---

## 🌐 HTTP 功能

### HTTP 服务端（API 接口）

系统启动后会在 `9090` 端口提供 HTTP API 服务，用于运行时控制推理功能。

### HTTP 客户端（外部调用）

系统集成了 HTTP 客户端功能，用于：
- **3D打印机控制**：当检测到异常时，自动向 Moonraker API 发送暂停打印命令
- **外部系统集成**：支持向其他 HTTP 服务发送通知或控制指令

> 💡 **技术实现**：使用 `cpp-httplib` 库进行 HTTP 请求，支持超时控制和错误处理

---

## 📡 MQTT 消息通知

系统集成了 MQTT 发布功能，当检测到缺陷时自动发送通知消息。

### 配置 Mosquitto Broker

```bash
# 安装
sudo apt install -y mosquitto mosquitto-clients libmosquitto-dev

# 配置 /etc/mosquitto/conf.d/local.conf
listener 1883 127.0.0.1
allow_anonymous true
persistence true
persistence_location /var/lib/mosquitto/
autosave_interval 60

# 启动服务
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
```

### MQTT 主题

| 主题 | 说明 |
|------|------|
| `opi/zero2/events/target_detected` | 检测到缺陷时发送 |

### 消息内容

```
检测到缺陷
```

### 订阅测试

```bash
mosquitto_sub -h 127.0.0.1 -t "opi/zero2/events/target_detected" -v
```

### 代码配置

在 `App.cpp` 中可修改 MQTT 配置：

```cpp
MqttPublisher::Config mqtt_config;
mqtt_config.broker_host = "127.0.0.1";
mqtt_config.broker_port = 1883;
mqtt_config.client_id = "ncnn_inference_server";
mqtt_config.topic = "opi/zero2/events/target_detected";
mqtt_config.qos = 1;
```

---

## 🔌 HTTP API 接口

### 推理开关控制

**POST** `/api/inference/toggle`

切换推理功能的开启/关闭状态。

**请求体：**
```json
{
    "enabled": true
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `enabled` | boolean | `true` 开启推理，`false` 关闭推理 |

**响应示例：**
```json
{
    "success": true,
    "message": "Inference enabled",
    "data": {
        "inference_enabled": true
    }
}
```

**使用示例：**

```bash
# 关闭推理
curl -X POST http://localhost:9090/api/inference/toggle \
  -H "Content-Type: application/json" \
  -d '{"enabled": false}'

# 开启推理
curl -X POST http://localhost:9090/api/inference/toggle \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

**说明：**
- 推理关闭后，文件监听仍然运行，但不会执行推理
- 状态切换即时生效，无需重启服务
- 默认状态为开启

---

## 📦 部署指南

详细的部署说明请参考 **[DEPLOYMENT.md](./DEPLOYMENT.md)**，包括：

- ✅ 系统要求和依赖安装
- ✅ 编译和配置
- ✅ Systemd 服务配置（自启动）
- ✅ 日志管理和故障排查
- ✅ 性能优化建议

项目交接请参考 **[Project_Handover.md](./Project_Handover.md)**，包含完整的项目交接信息。

---

## 🧪 批量推理工具

`scripts/batch_infer.py` 是一个独立的 Python 批量推理脚本，可用于模型测试、批量标注和结果可视化。

### 依赖安装

```bash
pip install numpy opencv-python ncnn
```

### 使用方法

```bash
# 基本用法（默认启用标注模块）
python scripts/batch_infer.py -i /path/to/images -o /path/to/labels

# 启用可视化模块（画图保存到 input/out 文件夹）
python scripts/batch_infer.py -i /path/to/images --visualize

# 同时启用标注和可视化
python scripts/batch_infer.py -i /path/to/images -o /path/to/labels --annotate --visualize

# 指定模型和参数
python scripts/batch_infer.py \
    -i /path/to/images \
    --param /path/to/model.param \
    --bin /path/to/model.bin \
    --conf 0.5 \
    --nms 0.45
```

### 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--input, -i` | `/home/torch/images` | 输入图片目录 |
| `--output, -o` | `/home/torch/labels` | 标注输出目录 |
| `--param` | 内置路径 | NCNN param 文件路径 |
| `--bin` | 内置路径 | NCNN bin 文件路径 |
| `--labels` | None | 标签文件路径（用于可视化显示类别名） |
| `--conf` | 0.5 | 置信度阈值 |
| `--nms` | 0.45 | NMS 阈值 |
| `--class-id` | 0 | 标注时使用的默认类别 ID |
| `--size` | 640 | 输入尺寸 |
| `--annotate` | False | 启用标注模块 |
| `--visualize` | False | 启用可视化模块 |

### 功能模块

**1. 标注模块 (--annotate)**
- 对输入目录中的所有图片执行推理
- 生成 YOLO 格式的 `.txt` 标注文件（归一化坐标）
- 输出到 `--output` 指定的目录

**2. 可视化模块 (--visualize)**
- 在图像上绘制检测框和置信度
- 保存到输入目录下的 `out/` 子文件夹
- 无检测结果的图片会跳过

### 典型使用场景

```bash
# 场景1：测试新模型是否正常工作
python scripts/batch_infer.py -i /path/to/test_images --visualize

# 场景2：为训练数据生成预标注
python scripts/batch_infer.py -i /path/to/train_images -o /path/to/labels --annotate

# 场景3：使用低置信度阈值检查模型召回率
python scripts/batch_infer.py -i /path/to/images --visualize --conf 0.3
```

---

## 👨‍💻 开发指南

### 添加新算法类型

1. **创建算法类** (继承 `algorithms::Algorithm`)：

```cpp
// src/algorithms/ClassificationAlgorithm.hpp
class ClassificationAlgorithm : public algorithms::Algorithm {
public:
    nlohmann::json infer(cv::Mat& image) override {
        // 实现推理逻辑
    }
    
    std::string getName() const override {
        return name_;
    }
};
```

2. **注册到工厂** (`AlgorithmFactory.hpp`)：

```cpp
if (config.type == "Classification") {
    return std::make_shared<ClassificationAlgorithm>(/*...参数*/);
}
```

3. **更新模型配置** (`model.json`)：

```json
{
    "type": "Classification",
    "name": "resnet50",
    "model_param": "./models/resnet50/model.param",
    "model_bin": "./models/resnet50/model.bin"
}
```

### 添加新预处理器

在 `src/algorithms/processors/ProcessorFactory.hpp` 中注册：

```cpp
if (preprocess_type == "custom_preprocess") {
    return std::make_shared<CustomPreprocessor>();
}
```

### 调试技巧

1. **启用详细日志**：
```cpp
spdlog::set_level(spdlog::level::debug);
```

2. **单步调试**：
```bash
gdb ./workspace/ncnn_demo
```

3. **性能分析**：
```bash
perf record -g ./workspace/ncnn_demo
perf report
```

---

## 🔧 常见问题

### Q1: 编译时找不到 NCNN 库

**解决方案**：确保对应架构的库目录存在：

```bash
# x86_64 编译检查
lib/amd/install_ncnn/
├── include/ncnn/
└── lib/libncnn.a

# aarch64 编译检查
lib/arm/install_ncnn/
├── include/ncnn/
└── lib/libncnn.a
```

如果缺失，需要手动编译 NCNN（以 aarch64 为例）：

```bash
git clone https://github.com/Tencent/ncnn.git
cd ncnn && mkdir build && cd build
cmake \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_INSTALL_PREFIX=../../lib/arm/install_ncnn \
    ..
make -j$(nproc) && make install
```

### Q2: 运行时无法检测到图像文件

**检查项**：
1. 确认监听目录存在：`ls -la /home/torch/images/pose`
2. 确认目录权限：`chmod 755 /home/torch/images/pose`
3. 查看日志输出，确认 FileWatcher 已启动
4. 测试手动复制文件：`cp test.jpg /home/torch/images/pose/`

### Q3: 推理速度慢

**优化建议**：
1. 使用 INT8 量化模型（参考 NCNN 文档）
2. 启用 Vulkan 加速（需要 GPU 支持）
3. 调整 OpenMP 线程数：`export OMP_NUM_THREADS=4`
4. 减少不必要的日志输出（使用 INFO 级别）

### Q4: 内存占用过高

**解决方案**：
1. 减少共享算法的并发数（修改 `ModelInstancePool` 的 `max_concurrent`）
2. 使用更小的模型（如 YOLO-Nano）
3. 启用独占算法模式，避免多实例

---

## 📄 许可证

MIT License - 详见 LICENSE 文件
