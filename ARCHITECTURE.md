# NCNN OpenCV Server - 架构设计文档

## 📚 目录

- [设计理念](#设计理念)
- [核心模块详解](#核心模块详解)
- [数据流程](#数据流程)
- [并发模型](#并发模型)
- [扩展指南](#扩展指南)

---

## 🎯 设计理念

### 核心原则

1. **模块化设计**：职责单一，松耦合，高内聚
2. **性能优先**：零拷贝、内存复用、并行计算
3. **可扩展性**：插件式算法、工厂模式

### 设计模式

- **工厂模式** (`AlgorithmFactory`): 动态创建算法实例
- **单例模式** (`AlgorithmManager`): 全局共享的算法管理器
- **观察者模式** (`FileWatcher`): 文件事件监听和通知
- **策略模式** (`Preprocessor/Postprocessor`): 灵活切换处理策略
- **对象池模式** (`ModelInstancePool`): 模型实例复用

---

## 🛠️ 核心模块详解

### 1. 应用层 (App.cpp)

**职责**：
- 系统生命周期管理
- 日志系统配置
- 主事件循环
- 推理结果回调处理

**启动流程**：

```
main() → 初始化日志 → run()
  ├─ 加载配置 (MyController)
  ├─ 创建 AlgorithmManager
  ├─ 创建 InferenceEngine
  ├─ 创建 MultiCameraManager
  ├─ 注册推理回调
  ├─ 启动所有相机
  └─ 进入主事件循环
```

---

### 2. 配置管理 (MyController)

**职责**：
- 加载和解析相机配置 (`config.json`)
- 配置数据结构转换
- 配置验证

**配置结构**：

```cpp
struct CameraConfig {
    std::string camera;                  // 相机 ID
    std::vector<std::string> algorithms; // 算法列表
    std::string watch_path;              // 监听路径
    bool has_watcher;                    // 是否启用监听
};
```

---

### 3. 多相机管理 (MultiCameraManager)

**职责**：
- 管理多个相机实例的生命周期
- 为每个相机创建和管理 FileWatcher
- 分发推理结果回调
- 协调相机和推理引擎

**相机实例结构**：

```cpp
struct CameraInstance {
    std::string camera_id;
    CameraConfig config;
    std::unique_ptr<FileWatcher> watcher;
    bool is_active;
};
```

**工作流程**：

```
文件事件 → FileWatcher → handleFileEvent()
  ├─ 读取图像文件
  ├─ inferFrame(camera_id, image)
  └─ 对每个结果调用 inference_callback_
```

---

### 4. 推理引擎 (InferenceEngine)

**职责**：
- 管理相机到算法的映射关系
- 协调共享算法和独占算法
- 执行推理任务
- 管理结果缓存

**算法模式对比**：

| 特性 | 共享算法 | 独占算法 |
|------|---------|----------|
| 内存占用 | 低 | 高 |
| 并发性能 | 需要锁 | 无锁 |
| 适用场景 | 小模型 | 大模型 |
| 实例数量 | 1个 | N个（N=相机数）|

**推理流程**：

```
inferFrame(camera_id, frame)
  └─ 对每个算法
      ├─ 判断共享/独占
      ├─ 获取算法实例
      ├─ 执行推理
      └─ 返回结果
```

---

### 5. 算法管理 (AlgorithmManager)

**职责**：
- 加载和管理算法配置 (`model.json`)
- 创建算法实例（通过 AlgorithmFactory）
- 统一的推理接口
- 线程安全的算法访问

**算法配置**：

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
};
```

---

### 6. 检测算法 (DetectionAlgorithm)

**职责**：
- 封装 YOLO 系列检测算法
- 管理预处理和后处理流水线
- 生成结构化检测结果
- 提供结果可视化

**处理流水线**：

```
输入图像 → Preprocessor → NCNN 推理 → Postprocessor → JSON 结果
```

**关键组件**：

- **Preprocessor**: 图像归一化、Resize、格式转换
- **NcnnInference**: NCNN 模型加载和推理
- **Postprocessor**: NMS、坐标映射、置信度过滤

---

### 7. NCNN 推理 (NcnnInference)

**职责**：
- NCNN 模型加载
- 前向推理
- Vulkan/CPU 后端支持
- INT8 量化支持

**接口**：

```cpp
class NcnnInference {
public:
    bool initialize();
    ncnn::Mat infer(const ncnn::Mat& input_tensor);
private:
    ncnn::Net NCNN_NET_;
    std::string input_layer_name_;
    std::string output_layer_name_;
};
```

---

### 8. 文件监听 (FileWatcher)

**职责**：
- Linux inotify 封装
- 文件事件过滤（CREATE/MODIFY/CLOSE_WRITE）
- 事件去重机制
- 异步回调通知

**事件处理**：

```cpp
struct FileEvent {
    EventType type;        // 事件类型
    std::string file_path; // 完整路径
    std::string file_name; // 文件名
};

using EventCallback = std::function<void(const FileEvent&)>;
```

---

## 📊 数据流程

### 完整推理流程

```
1. 新图像文件 → 监听目录
   ↓
2. FileWatcher 检测到 CLOSE_WRITE 事件
   ↓
3. 触发 handleFileEvent() 回调
   ↓
4. 读取图像文件 (cv::imread)
   ↓
5. InferenceEngine::inferFrame(camera_id, image)
   ↓
6. 根据相机配置获取算法列表
   ↓
7. 对每个算法：
   ├─ AlgorithmManager::infer(alg_name, image)
   ├─ DetectionAlgorithm::infer(image)
   ├─ Preprocessor::process(image)
   ├─ NcnnInference::infer(tensor)
   ├─ Postprocessor::process(output)
   └─ 返回 JSON 结果
   ↓
8. 收集所有算法的结果
   ↓
9. 触发 inference_callback_(camera_id, alg_name, image, result)
   ↓
10. handleInferenceResult()
    ├─ 输出 JSON 到控制台
    ├─ 保存可视化结果
    └─ 可选：调用外部 API
```

---

## 🔄 并发模型

### 线程结构

```
主线程 (main)
  ├─ 事件循环（每60s检查一次）
  └─ 推理结果处理

文件监听线程 (FileWatcher)
  ├─ inotify 事件监听
  └─ 触发推理回调

推理线程池 (OpenMP)
  ├─ 并行图像预处理
  └─ NCNN 推理（多线程）
```

### 同步机制

| 资源 | 保护机制 | 说明 |
|------|----------|------|
| `AlgorithmManager::algorithms_` | `algorithm_mutexes_` | 每个算法独立锁 |
| `MultiCameraManager::cameras_` | `mutex_` | 相机实例访问 |
| `InferenceEngine::mapping_` | `mapping_mutex_` | 相机-算法映射 |
| `InferenceEngine::cache_` | `cache_mutex_` | 结果缓存 |

### 无锁设计

- **独占算法模式**：每个相机独立实例，无需锁
- **配置只读**：启动后配置不变，无需锁
- **文件监听**：每个相机独立 FileWatcher，无竞争

---

## 🔧 扩展指南

### 添加新算法类型

#### 1. 创建算法类

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

#### 2. 注册到工厂

```cpp
// src/algorithms/AlgorithmFactory.hpp
if (config.type == "Classification") {
    return std::make_shared<ClassificationAlgorithm>(/* 参数 */);
}
```

#### 3. 更新配置

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
// src/algorithms/processors/CustomPreprocessor.hpp
class CustomPreprocessor : public Preprocessor {
public:
    PreprocessResult process(const cv::Mat& image) override {
        // 实现自定义预处理逻辑
    }
};

// src/algorithms/processors/ProcessorFactory.hpp
if (preprocess_type == "custom") {
    return std::make_shared<CustomPreprocessor>();
}
```

### 添加新后处理器

```cpp
// src/algorithms/processors/CustomPostprocessor.hpp
class CustomPostprocessor : public Postprocessor {
public:
    Result process(
        const ncnn::Mat& output,
        const PreprocessResult& preprocess_result) override {
        // 实现自定义后处理逻辑
    }
};
```

### 扩展文件监听

```cpp
// 添加新的文件过滤规则
FileWatcher watcher(path, {".jpg", ".png", ".bmp"});

// 自定义事件处理
watcher.start([](const FileWatcher::FileEvent& event) {
    if (event.type == FileWatcher::EventType::FILE_CLOSED) {
        // 处理文件
    }
});
```
---
## 🎓 总结

本系统采用模块化、分层的架构设计，通过清晰的职责划分和接口定义，实现了：

- ✅ 高性能：NCNN 优化、并行计算、内存复用
- ✅ 高并发：多相机独立推理、无锁设计
- ✅ 可扩展：插件式算法、工厂模式、策略模式
- ✅ 可维护：模块化设计、完善日志、错误处理
- ✅ 跨平台：支持 x86_64 和 aarch64 架构，统一构建脚本

---

## 📦 依赖库管理

项目采用预编译库方式管理核心依赖，按 CPU 架构分类存放：

```
lib/
├── amd/                        # x86_64 架构预编译库
│   ├── install_ncnn/           # NCNN 推理框架
│   ├── install_opencv/         # OpenCV 图像处理
│   ├── install_spdlog/         # 日志库
│   ├── install_inotify/        # 文件监听库
│   └── httplib.h               # HTTP 库（头文件）
└── arm/                        # aarch64 架构预编译库
    ├── install_ncnn/
    ├── install_opencv/
    ├── install_spdlog/
    ├── install_inotify/
    └── httplib.h
```

构建时通过 `LIB_ARCH` 参数自动选择对应架构的库：
- `./build.sh x86` → 使用 `lib/amd/`
- `./build.sh arm` → 使用 `lib/arm/`