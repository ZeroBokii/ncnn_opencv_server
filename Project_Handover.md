# NCNN OpenCV Server 项目交接文档

> **文档版本**: 1.0  
> **生成日期**: 2026-01-30  
> **项目版本**: v2.0  
> **原始开发者**: Torch

---

## 🎯 项目概述

### 项目名称
**NCNN OpenCV 智能推理服务器**

### 项目描述
这是一个基于 NCNN + OpenCV 的高性能多相机多算法实时推理系统，专为边缘设备和生产环境设计。系统通过文件监听（inotify）自动检测新图像并触发 AI 推理，支持多种算法类型（检测、分类、分割），并提供 HTTP API 控制接口和 MQTT 消息通知功能。

### 主要应用场景
- **3D 打印监控**：实时检测打印故障（如面条检测）
- **工业视觉检测**：质量控制、缺陷检测
- **智能监控系统**：多摄像头目标检测
- **边缘 AI 推理服务**

### 核心特性
| 特性 | 说明 |
|------|------|
| 多相机并发推理 | 同时管理多个图像源，独立配置算法 |
| 多算法协同 | 灵活组合不同 AI 算法 |
| 文件监听驱动 | 基于 inotify 自动检测新图像 |
| 高性能推理 | NCNN 优化，支持 INT8 量化，OpenMP 多线程 |
| 热加载支持 | 运行时动态更新配置和模型 |
| HTTP API 控制 | REST API 动态控制推理开关 |
| MQTT 消息通知 | 检测到缺陷时自动发送通知 |
| 跨平台编译 | 支持 x86_64 和 aarch64 架构 |

---

## 🛠️ 技术栈总览

| 类别 | 技术 | 版本要求 | 用途 |
|------|------|----------|------|
| **编程语言** | C++ | C++20 | 核心开发语言 |
| **编译器** | GCC | >= 9.0 | 支持 C++20 特性 |
| **构建系统** | CMake | >= 3.10 | 项目构建管理 |
| **推理框架** | NCNN | latest | 深度学习推理 |
| **计算机视觉** | OpenCV | >= 4.0 | 图像处理 |
| **日志库** | spdlog | latest | 高性能日志 |
| **JSON 解析** | nlohmann_json | >= 3.0 | 配置和结果序列化 |
| **文件监听** | inotify-cpp | latest | Linux 文件监控 |
| **HTTP 库** | cpp-httplib | latest | REST API 服务 |
| **MQTT 库** | libmosquitto | latest | MQTT 消息发布 |
| **并行计算** | OpenMP | latest | 多线程加速 |

### 系统要求
- **操作系统**: Ubuntu 20.04+ / Debian 11+ 或其他主流 Linux 发行版
- **CPU**: x86_64 或 aarch64 架构
- **内存**: 至少 4GB RAM
- **编译器**: GCC >= 9.0 (支持 C++20)
- **CMake**: >= 3.10

---

## 🧩 核心模块说明

### 1. 应用层 (App.cpp)

**职责**：系统生命周期管理、日志配置、主事件循环

**启动流程**：
```
main()
 ├─ 配置日志系统（控制台 + 延迟文件）
 └─ run()
     ├─ 显示启动 Banner
     ├─ 初始化 MQTT 发布器
     ├─ 加载相机配置 (MyController)
     ├─ 收集所需算法列表
     ├─ 创建 AlgorithmManager
     ├─ 创建 InferenceEngine
     ├─ 创建 MultiCameraManager
     ├─ 注册推理结果回调
     ├─ 初始化并启动所有相机
     ├─ 启动 HTTP API 服务器 (端口 9090)
     └─ 进入主事件循环（每 60s 输出状态）
```

**重要函数**：
- `handleInferenceResult()`: 推理结果处理，包含可视化保存、MQTT 发送、打印机暂停逻辑

### 2. 配置管理 (MyController)

**职责**：加载和解析配置文件

**配置数据结构**：
```cpp
struct CameraConfig {
    std::string camera;                  // 相机 ID
    std::vector<std::string> algorithms; // 算法列表
    std::string watch_path;              // 监听路径
    bool has_watcher;                    // 是否启用监听
};
```

### 3. 多相机管理 (MultiCameraManager)

**职责**：
- 管理多个相机实例的生命周期
- 为每个相机创建和管理 FileWatcher
- 分发推理结果回调

**关键接口**：
```cpp
int initializeCameras(config);      // 初始化所有相机
bool startCamera(camera_id);        // 启动指定相机
int startAllCameras();              // 启动所有相机
void setInferenceCallback(func);    // 设置推理回调
```

### 4. 推理引擎 (InferenceEngine)

**职责**：
- 管理相机到算法的映射关系
- 协调共享算法和独占算法
- 执行推理任务

**算法模式**：
| 特性 | 共享算法 | 独占算法 |
|------|---------|----------|
| 内存占用 | 低 | 高 |
| 并发性能 | 需要锁 | 无锁 |
| 适用场景 | 小模型 | 大模型 |

### 5. 算法管理 (AlgorithmManager)

**职责**：
- 加载模型配置 (`model.json`)
- 创建算法实例（通过 AlgorithmFactory）
- 统一的推理接口

**算法配置结构**：
```cpp
struct AlgorithmConfig {
    std::string type;         // Detection/Classification/Segmentation
    std::string name;         // 算法名称
    std::string model_param;  // NCNN param 文件
    std::string model_bin;    // NCNN bin 文件
    std::string label_path;   // 标签文件
    std::string preprocess;   // 预处理类型
    std::string in_name;      // 输入层名
    std::string out_name;     // 输出层名
    bool use_int8;            // 是否使用 INT8 量化
};
```

### 6. NCNN 推理 (NcnnInference)

**职责**：NCNN 模型加载和前向推理

**关键接口**：
```cpp
bool initialize();                     // 初始化模型
ncnn::Mat infer(const ncnn::Mat& input); // 执行推理
```

### 7. 文件监听 (FileWatcher)

**职责**：基于 Linux inotify 的文件监控

**特性**：
- 监听 CREATE/MODIFY/CLOSE_WRITE 事件
- 文件扩展名过滤
- 事件去重机制（防止重复处理）

### 8. HTTP API 服务器 (HttpApiServer)

**职责**：提供 REST API 控制接口

**端口**：9090

### 9. MQTT 发布器 (MqttPublisher)

**职责**：
- 连接 MQTT Broker
- 发送缺陷检测通知
- 自动断线重连（1~10秒指数回退）

---

## 🔨 编译与构建

### 安装系统依赖

```bash
# 更新包管理器
sudo apt-get update

# 安装编译工具
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git

# 安装运行时依赖
sudo apt-get install -y \
    nlohmann-json3-dev \
    libomp-dev \
    libboost-dev \
    libmosquitto-dev

# 交叉编译额外依赖（仅 aarch64 需要）
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu
```

### 编译命令

```bash
# x86_64 本地编译（开发调试）
./build.sh x86

# aarch64 交叉编译（嵌入式部署）
./build.sh arm

# 清理后重新编译
./build.sh x86 clean
./build.sh arm clean

# 清理所有构建目录
./build.sh clean
```

### 编译输出
编译成功后，可执行文件生成在 `workspace/ncnn_opencv_server`

### CMake 关键配置

| 参数 | 说明 |
|------|------|
| `LIB_ARCH=amd` | 使用 x86_64 预编译库 |
| `LIB_ARCH=arm` | 使用 aarch64 预编译库 |
| `CMAKE_BUILD_TYPE=Release` | 发布版本 |

---

## ⚙️ 配置文件说明

### 1. 相机配置 (`workspace/configs/config.json`)

```json
{
    "config_mapping": {
        "camera_top": {
            "alg": ["noodle"],
            "watcher": "/home/torch/images"
        },
        "camera_left": {
            "alg": ["detection", "classification"],
            "watcher": "/home/user/images/left"
        }
    }
}
```

**字段说明**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `camera_id` | string | 相机唯一标识符（如 `camera_top`） |
| `alg` | array | 该相机使用的算法名称列表 |
| `watcher` | string | 监听的图像目录路径 |

### 2. 模型配置 (`workspace/models/model.json`)

```json
{
    "models": [
        {
            "type": "Detection",
            "name": "noodle",
            "model_param": "./models/3dv26n-cfg-v1/best.ncnn.param",
            "model_bin": "./models/3dv26n-cfg-v1/best.ncnn.bin",
            "label_path": "./models/3dv11n-cfg-v4/label.txt",
            "preprocess": "yolo11",
            "in_name": "in0",
            "out_name": "out0",
            "int8": false
        }
    ]
}
```

**字段说明**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 算法类型：`Detection`/`Classification`/`Segmentation` |
| `name` | string | 算法唯一名称，与 config.json 中的 `alg` 对应 |
| `model_param` | string | NCNN 模型参数文件路径 (.param) |
| `model_bin` | string | NCNN 模型权重文件路径 (.bin) |
| `label_path` | string | 类别标签文件路径 |
| `preprocess` | string | 预处理类型：`yolo11`/`yolov8`/`yolov5` |
| `in_name` | string | NCNN 输入层名称（通常为 `in0`） |
| `out_name` | string | NCNN 输出层名称（通常为 `out0`） |
| `int8` | bool | 是否使用 INT8 量化 |

### 3. MQTT 配置（在 App.cpp 中硬编码）

```cpp
MqttPublisher::Config mqtt_config;
mqtt_config.broker_host = "127.0.0.1";
mqtt_config.broker_port = 1883;
mqtt_config.client_id = "ncnn_inference_server";
mqtt_config.topic = "opi/zero2/events/target_detected";
mqtt_config.qos = 1;
```

---

## 🚀 部署流程

### 方式一：直接运行

```bash
cd workspace
./ncnn_opencv_server
```

### 方式二：部署为系统服务

```bash
# 部署为 systemd 服务（需要 root 权限）
sudo ./scripts/start.sh
```

### 服务管理命令

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

### 嵌入式设备部署

```bash
# 1. 在开发机上交叉编译
./build.sh arm

# 2. 打包部署文件
tar -czvf ncnn_server_arm.tar.gz \
    --transform 's,^,ncnn_opencv_server/,' \
    workspace/ncnn_opencv_server \
    workspace/configs \
    workspace/models \
    lib/arm \
    scripts/start.sh

# 3. 传输到目标设备
scp ncnn_server_arm.tar.gz user@target_ip:/home/user/

# 4. 在目标设备上解压并部署
tar -xzvf ncnn_server_arm.tar.gz
cd ncnn_opencv_server
sudo ./scripts/start.sh
```

---

## 🔌 API 接口

### HTTP API

**服务端口**: 9090

#### 推理开关控制

**POST** `/api/inference/toggle`

切换推理功能的开启/关闭状态。

**请求体**：
```json
{
    "enabled": true
}
```

**响应**：
```json
{
    "success": true,
    "message": "Inference enabled",
    "data": {
        "inference_enabled": true
    }
}
```

**使用示例**：
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

### MQTT 接口

**Broker**: 127.0.0.1:1883  
**Topic**: `opi/zero2/events/target_detected`  
**消息内容**: `"检测到缺陷"`

**订阅测试**：
```bash
mosquitto_sub -h 127.0.0.1 -t "opi/zero2/events/target_detected" -v
```

---

## 📍 关键代码位置

### 需要重点了解的文件

| 文件 | 路径 | 重要性 | 说明 |
|------|------|--------|------|
| 主程序入口 | [src/App.cpp](src/App.cpp) | ⭐⭐⭐ | 系统启动流程、推理回调 |
| 相机配置 | [workspace/configs/config.json](workspace/configs/config.json) | ⭐⭐⭐ | 相机和算法映射 |
| 模型配置 | [workspace/models/model.json](workspace/models/model.json) | ⭐⭐⭐ | 模型参数配置 |
| 推理引擎 | [src/inferEngine/InferenceEngine.hpp](src/inferEngine/InferenceEngine.hpp) | ⭐⭐⭐ | 核心推理逻辑 |
| 算法工厂 | [src/algorithms/AlgorithmFactory.hpp](src/algorithms/AlgorithmFactory.hpp) | ⭐⭐ | 添加新算法时修改 |
| 多相机管理 | [src/camera/MultiCameraManager.hpp](src/camera/MultiCameraManager.hpp) | ⭐⭐ | 相机生命周期管理 |
| 构建脚本 | [build.sh](build.sh) | ⭐⭐ | 编译命令 |
| 部署脚本 | [scripts/start.sh](scripts/start.sh) | ⭐⭐ | 服务部署 |
| CMake 配置 | [CMakeLists.txt](CMakeLists.txt) | ⭐⭐ | 构建配置 |

### 扩展开发时需要修改的文件

| 场景 | 需要修改的文件 |
|------|----------------|
| 添加新算法类型 | `AlgorithmFactory.hpp`, `model.json` |
| 添加新预处理器 | `ProcessorFactory.hpp`, 新建处理器类 |
| 修改 MQTT 配置 | `App.cpp` |
| 修改 HTTP API | `HttpApiServer.cpp` |
| 修改推理回调逻辑 | `App.cpp` 中的 `handleInferenceResult()` |

---

### 配置更新流程

1. 修改配置文件（`config.json` 或 `model.json`）
2. 重启服务：`sudo systemctl restart ncnn-server`
3. 验证配置生效：查看日志确认

### 模型更新流程

1. 将新模型文件放入 `workspace/models/<model_folder>/`
2. 更新 `model.json` 中的路径
3. 重启服务

---

## 🧪 批量推理工具 (batch_infer.py)

`scripts/batch_infer.py` 是一个独立的 Python 批量推理脚本，可用于：
- **模型测试**：验证 NCNN 模型是否正常工作
- **批量标注**：自动生成 YOLO 格式标注文件
- **结果可视化**：批量生成带检测框的图像

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

#### 1. 标注模块 (--annotate)
- 对输入目录中的所有图片执行推理
- 生成 YOLO 格式的 `.txt` 标注文件
- 输出到 `--output` 指定的目录

**YOLO 标注格式**：
```
<class_id> <x_center> <y_center> <width> <height>
```
所有坐标为归一化值（0~1）

#### 2. 可视化模块 (--visualize)
- 对输入目录中的所有图片执行推理
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

## � 补充说明

### 推理结果输出格式

推理结果以 JSON 格式输出到控制台，结构如下：

```json
{
    "camera_id": "camera_top",
    "algorithm": "noodle",
    "result": {
        "time_ms": 45,
        "detections": [
            {
                "box": {
                    "left": 120.50,
                    "top": 80.25,
                    "right": 350.75,
                    "bottom": 450.00
                },
                "cls_id": 0,
                "score": 0.95,
                "label": "noodle"
            }
        ]
    }
}
```

**字段说明**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `camera_id` | string | 相机标识符 |
| `algorithm` | string | 算法名称 |
| `time_ms` | int | 推理耗时（毫秒） |
| `detections` | array | 检测结果数组 |
| `box` | object | 边界框坐标（像素值） |
| `cls_id` | int | 类别 ID |
| `score` | float | 置信度（0~1） |
| `label` | string | 类别名称 |

### 可视化结果输出

当检测到目标时，系统会自动保存可视化图像：

- **保存位置**：`workspace/output_results/`
- **命名格式**：`{camera_id}_{algorithm}_{timestamp}.jpg`
- **绘制内容**：检测框、类别名称、置信度

### 日志系统

**日志位置**：`workspace/logs/`

**日志文件命名**：`log_YYYY-MM-DD_HH-MM-SS.txt`

**日志级别**：
- 控制台：DEBUG 级别（显示所有日志）
- 文件：ERROR 级别（仅记录错误，延迟创建）

**延迟文件机制**：日志文件仅在首次出现 ERROR 级别日志时才会创建，避免产生大量空日志文件。

### 支持的图像格式

FileWatcher 默认支持以下图像格式：
- `.jpg` / `.jpeg`
- `.png`
- `.bmp`

### 模型文件结构

每个模型文件夹应包含以下文件：

```
workspace/models/<model_name>/
├── best.ncnn.param    # NCNN 模型参数文件
├── best.ncnn.bin      # NCNN 模型权重文件
└── label.txt          # 类别标签文件（每行一个类别名）
```

**label.txt 示例**：
```
noodle
normal
defect
```

### manage_arm64.sh 脚本说明

`scripts/manage_arm64.sh` 用于管理 arm64 交叉编译环境：

```bash
# 开启 arm64 交叉编译环境（默认）
./scripts/manage_arm64.sh true

# 关闭 arm64 交叉编译环境
./scripts/manage_arm64.sh false
```

**功能说明**：
- **开启**：添加 arm64 架构，安装交叉编译依赖库（如 `libmosquitto-dev:arm64`）
- **关闭**：移除所有 arm64 软件包和架构配置

### 注释功能说明

代码中有部分功能被注释，可根据需要启用：

**1. MQTT 缺陷通知**（`src/App.cpp` 第 48-51 行）
```cpp
// 取消注释以启用 MQTT 通知
if (g_mqtt_publisher) {
    g_mqtt_publisher->publishDefectDetected();
}
```

**2. 打印机暂停功能**（`src/App.cpp` 第 53-70 行）
```cpp
// 取消注释以启用打印机暂停（需要 Moonraker API）
// 包含 5 秒冷却时间防止重复发送
```

**启用方法**：在 `src/App.cpp` 的 `handleInferenceResult()` 函数中取消相关代码的注释，然后重新编译。

---

## �🔄 扩展开发指南

### 添加新算法类型

#### 步骤 1: 创建算法类

```cpp
// src/algorithms/ClassificationAlgorithm.hpp
class ClassificationAlgorithm : public algorithms::Algorithm {
public:
    ClassificationAlgorithm(/* 参数 */);
    nlohmann::json infer(cv::Mat& image) override;
    std::string getName() const override;
private:
    // 成员变量
};
```

#### 步骤 2: 注册到工厂

```cpp
// src/algorithms/AlgorithmFactory.hpp
if (config.type == "Classification") {
    return std::make_shared<ClassificationAlgorithm>(/* 参数 */);
}
```

#### 步骤 3: 更新模型配置

```json
// workspace/models/model.json
{
    "type": "Classification",
    "name": "resnet50",
    "model_param": "./models/resnet50/model.param",
    "model_bin": "./models/resnet50/model.bin"
}
```

### 添加新预处理器

```cpp
// 1. 创建新预处理器类
// src/algorithms/processors/CustomPreprocessor.hpp

// 2. 注册到 ProcessorFactory
// src/algorithms/processors/ProcessorFactory.hpp
if (preprocess_type == "custom") {
    return std::make_shared<CustomPreprocessor>();
}
```

---

## 📚 相关文档索引

| 文档 | 路径 | 内容 |
|------|------|------|
| 项目说明 | [README.md](README.md) | 项目概述、快速开始、完整配置说明 |
| 架构设计 | [ARCHITECTURE.md](ARCHITECTURE.md) | 详细架构设计、数据流程、并发模型 |
| 部署指南 | [DEPLOYMENT.md](DEPLOYMENT.md) | 系统要求、编译、服务部署 |

---

### 原始开发者
- **开发者**: Torch
- **项目版本**: v2.0

> **文档更新日期**: 2026-01-30  