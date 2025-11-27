#include "MultiCameraManager.hpp"
#include <chrono>
#include <thread>

MultiCameraManager::MultiCameraManager(std::shared_ptr<InferenceEngine> inference_engine)
    : inference_engine_(inference_engine) {
    
    if (!inference_engine_) {
        throw std::runtime_error("MultiCameraManager: inference_engine cannot be null");
    }
    
    spdlog::info("MultiCameraManager created");
}

MultiCameraManager::~MultiCameraManager() {
    stopAllCameras();
    spdlog::info("MultiCameraManager destroyed");
}

int MultiCameraManager::initializeCameras(
    const std::unordered_map<std::string, CameraConfig>& camera_configs) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int success_count = 0;
    
    for (const auto& [camera_id, config] : camera_configs) {
        if (!validateCameraAlgorithms(config)) {
            spdlog::error("Camera '{}' algorithm verification failed, skip initialization", camera_id);
            continue;
        }
        
        CameraInstance instance;
        instance.camera_id = camera_id;
        instance.config = config;
        instance.is_active = false;
        
        if (config.has_watcher && !config.watch_path.empty()) {
            try {
                instance.watcher = std::make_unique<FileWatcher>(
                    config.watch_path,
                    std::vector<std::string>{".jpg", ".jpeg", ".png", ".bmp", ".webp"}
                );
            } catch (const std::exception& e) {
                spdlog::error("  ✗ 创建文件监听器失败: {}", e.what());
                // 监听器创建失败不影响相机初始化
            }
        }
        
        cameras_[camera_id] = std::move(instance);
        success_count++;
        
        // 拼接算法列表
        std::string alg_list;
        for (size_t i = 0; i < config.algorithms.size(); ++i) {
            alg_list += config.algorithms[i];
            if (i < config.algorithms.size() - 1) {
                alg_list += ", ";
            }
        }
        
        spdlog::info("✓ Camera '{}' initialized, algorithms: [{}]", camera_id, alg_list);
    }
    
    return success_count;
}

bool MultiCameraManager::startCamera(const std::string& camera_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cameras_.find(camera_id);
    if (it == cameras_.end()) {
        spdlog::error("相机 '{}' 不存在", camera_id);
        return false;
    }
    
    CameraInstance& instance = it->second;
    
    if (instance.is_active) {
        spdlog::warn("相机 '{}' 已经在运行中", camera_id);
        return true;
    }
    
    // 启动文件监听器
    if (instance.watcher) {
        bool started = instance.watcher->start(
            [this, camera_id](const FileWatcher::FileEvent& event) {
                handleFileEvent(camera_id, event);
            }
        );
        
        if (!started) {
            spdlog::error("启动相机 '{}' 的文件监听器失败", camera_id);
            return false;
        }
    }
    
    instance.is_active = true;
    
    return true;
}

void MultiCameraManager::stopCamera(const std::string& camera_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cameras_.find(camera_id);
    if (it == cameras_.end()) {
        return;
    }
    
    CameraInstance& instance = it->second;
    
    if (!instance.is_active) {
        return;
    }
    
    if (instance.watcher) {
        instance.watcher->stop();
    }
    
    instance.is_active = false;
}

int MultiCameraManager::startAllCameras() {
    int success_count = 0;
    
    // 获取所有相机ID（避免在锁内调用 startCamera）
    std::vector<std::string> camera_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : cameras_) {
            camera_ids.push_back(id);
        }
    }
    
    // 启动每个相机
    for (const auto& camera_id : camera_ids) {
        if (startCamera(camera_id)) {
            success_count++;
        }
    }
    
    spdlog::info("Cameras started: {}/{} successful", success_count, camera_ids.size());
    
    return success_count;
}

void MultiCameraManager::stopAllCameras() {
    spdlog::info("停止所有相机...");
    
    std::vector<std::string> camera_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : cameras_) {
            camera_ids.push_back(id);
        }
    }
    
    for (const auto& camera_id : camera_ids) {
        stopCamera(camera_id);
    }
    
    spdlog::info("所有相机已停止");
}

std::vector<nlohmann::json> MultiCameraManager::inferImage(
    const std::string& camera_id,
    const cv::Mat& image) {
    
    if (image.empty()) {
        spdlog::error("输入图像为空");
        return {};
    }
    
    // 验证相机是否存在
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cameras_.find(camera_id) == cameras_.end()) {
            spdlog::error("相机 '{}' 不存在", camera_id);
            return {};
        }
    }
    
    // 通过 InferenceEngine 执行推理
    std::vector<nlohmann::json> results = inference_engine_->inferFrame(camera_id, image);
    
    // 调用回调函数（使用图像克隆避免 Use-After-Free）
    if (inference_callback_) {
        // 克隆图像以确保回调函数持有独立副本
        cv::Mat image_clone = image.clone();
        
        for (const auto& result : results) {
            std::string algorithm_name = result["algorithm"].get<std::string>();
            nlohmann::json alg_result = result["result"];
            
            inference_callback_(camera_id, algorithm_name, image_clone, alg_result);
        }
    }
    
    return results;
}

void MultiCameraManager::setInferenceCallback(InferenceCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    inference_callback_ = callback;
}

const CameraInstance* MultiCameraManager::getCameraInstance(const std::string& camera_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cameras_.find(camera_id);
    if (it == cameras_.end()) {
        return nullptr;
    }
    
    return &it->second;
}

bool MultiCameraManager::hasCamera(const std::string& camera_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cameras_.find(camera_id) != cameras_.end();
}

int MultiCameraManager::getActiveCameraCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int count = 0;
    for (const auto& [_, instance] : cameras_) {
        if (instance.is_active) {
            count++;
        }
    }
    
    return count;
}

std::vector<std::string> MultiCameraManager::getAllCameraIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(cameras_.size());
    
    for (const auto& [id, _] : cameras_) {
        ids.push_back(id);
    }
    
    return ids;
}

void MultiCameraManager::handleFileEvent(
    const std::string& camera_id, 
    const FileWatcher::FileEvent& event) {
    
    // 只处理文件关闭事件（写入完成）
    if (event.type != FileWatcher::EventType::FILE_CLOSED) {
        return;
    }
    
    spdlog::info("Camera '{}' new image: {}", camera_id, event.file_name);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    cv::Mat image;
    int max_retries = 3;
    for (int i = 0; i < max_retries; ++i) {
        image = cv::imread(event.file_path);
        if (!image.empty()) {
            break;
        }
        if (i < max_retries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (image.empty()) {
        return;
    }
    
    std::vector<nlohmann::json> results = inferImage(camera_id, image);
}

bool MultiCameraManager::validateCameraAlgorithms(const CameraConfig& config) const {
    if (config.algorithms.empty()) {
        spdlog::warn("相机 '{}' 未配置任何算法", config.camera);
        return false;
    }
    
    spdlog::debug("相机 '{}' 配置了 {} 个算法", config.camera, config.algorithms.size());
    return true;
}