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
| **HTTP 客户端** | libcurl | latest | 外部 API 调用 |
| **并行计算** | OpenMP | latest | 多线程加速 |

---

## 📁 目录结构

```
ncnn_opencv_server/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明文档（本文件）
├── DEPLOYMENT.md               # 部署和服务配置指南
├── ARCHITECTURE.md             # 详细架构设计文档
│
├── src/                        # 源代码目录
│   ├── App.cpp                # 主程序入口
│   │
│   ├── common/                # 公共模块
│   │   ├── MyController.hpp   # 配置加载控制器
│   │   └── MyController.cpp
│   │
│   ├── camera/                # 相机管理模块
│   │   ├── CameraManager.hpp  # 相机配置结构
│   │   ├── MultiCameraManager.hpp  # 多相机管理器
│   │   └── MultiCameraManager.cpp
│   │
│   ├── algorithms/            # 算法管理模块
│   │   ├── Algorithm.hpp      # 算法基类接口
│   │   ├── AlgorithmFactory.hpp    # 算法工厂
│   │   ├── AlgorithmManager.hpp    # 算法管理器
│   │   ├── AlgorithmManager.cpp
│   │   ├── DetectionAlgorithm.hpp  # 检测算法实现
│   │   └── processors/        # 预处理/后处理器
│   │       ├── ProcessorFactory.hpp
│   │       ├── Preprocessor.hpp
│   │       └── Postprocessor.hpp
│   │
│   ├── inferEngine/           # 推理引擎模块
│   │   ├── InferenceEngine.hpp     # 推理引擎主类
│   │   ├── InferenceEngine.cpp
│   │   ├── ModelInstancePool.hpp   # 共享算法池
│   │   ├── ModelInstancePool.cpp
│   │   ├── ExclusiveInstanceManager.hpp  # 独占算法管理
│   │   └── ExclusiveInstanceManager.cpp
│   │
│   ├── deploy/                # NCNN 推理封装
│   │   ├── NcnnInference.hpp  # NCNN 推理接口
│   │   └── NcnnInference.cpp
│   │
│   ├── inotify/               # 文件监听模块
│   │   ├── FileWatcher.hpp    # 文件监听器
│   │   └── FileWatcher.cpp
│   │
│   └── tools/                 # 工具类
│       ├── Utils.hpp          # 通用工具函数
│       ├── Utils.cpp
│       └── LazyFileSink.hpp   # 延迟日志文件 sink
│
├── lib/                       # 第三方库
│   ├── install_ncnn/          # NCNN 库文件
│   ├── install_inotify/       # inotify-cpp 库文件
│   └── install_spdlog/        # spdlog 库文件
│
├── build/                     # CMake 构建输出目录
│
└── workspace/                 # 运行时工作目录
    ├── ncnn_demo              # 编译后的可执行文件
    ├── configs/               # 配置文件
    │   └── config.json        # 相机配置
    ├── models/                # 模型文件
    │   ├── model.json         # 模型配置
    │   └── yolo11n/           # YOLO11 模型
    │       ├── int8.param     # NCNN 参数文件
    │       ├── int8.bin       # NCNN 权重文件
    │       └── label.txt      # 类别标签
    ├── logs/                  # 日志文件（自动生成）
    └── output_results/        # 推理结果输出（自动生成）
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
    libopencv-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    libomp-dev
```

### 3. 编译项目

```bash
# 进入项目目录
cd ncnn_opencv_server

# 创建并进入构建目录
mkdir -p build && cd build

# 配置 CMake
cmake ..

# 编译（使用多核加速）
make -j$(nproc)
```

编译成功后，可执行文件位于 `workspace/ncnn_demo`。

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
# 进入工作目录
cd workspace

# 运行服务
./ncnn_demo
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

## 📦 部署指南

详细的部署说明请参考 **[DEPLOYMENT.md](./DEPLOYMENT.md)**，包括：

- ✅ 系统要求和依赖安装
- ✅ 编译和配置
- ✅ Systemd 服务配置（自启动）
- ✅ 日志管理和故障排查
- ✅ 性能优化建议

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

**解决方案**：确保 `lib/install_ncnn/` 目录存在且包含 NCNN 编译产物：

```bash
lib/install_ncnn/
├── include/ncnn/
└── lib/libncnn.a
```

如果缺失，需要手动编译 NCNN：

```bash
git clone https://github.com/Tencent/ncnn.git
cd ncnn
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=../../lib/install_ncnn ..
make -j$(nproc)
make install
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
