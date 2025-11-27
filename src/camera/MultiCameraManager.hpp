//
// Created for ncnn_demo - Multi-Camera Manager
// 多相机管理器 - 管理多个相机实例、算法和文件监听器
//

#ifndef MULTI_CAMERA_MANAGER_HPP
#define MULTI_CAMERA_MANAGER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "CameraManager.hpp"
#include "../algorithms/AlgorithmManager.hpp"
#include "../inferEngine/InferenceEngine.hpp"
#include "../inotify/FileWatcher.hpp"

/**
 * @brief 相机实例结构 - 包含单个相机的所有运行时信息
 */
struct CameraInstance {
    std::string camera_id;                              // 相机ID
    CameraConfig config;                                // 相机配置
    std::unique_ptr<FileWatcher> watcher;               // 文件监听器（如果配置了监听）
    bool is_active;                                     // 是否激活
    
    CameraInstance()
        : is_active(false) {}
};

/**
 * @brief 推理结果回调函数类型
 * @param camera_id 相机ID
 * @param algorithm_name 算法名称
 * @param image 输入图像
 * @param result 推理结果（JSON格式）
 */
using InferenceCallback = std::function<void(
    const std::string& camera_id,
    const std::string& algorithm_name,
    const cv::Mat& image,
    const nlohmann::json& result
)>;

class MultiCameraManager {
public:
    explicit MultiCameraManager(std::shared_ptr<InferenceEngine> inference_engine);
    ~MultiCameraManager();
    
    // 禁用拷贝
    MultiCameraManager(const MultiCameraManager&) = delete;
    MultiCameraManager& operator=(const MultiCameraManager&) = delete;
    
    /**
     * @brief 从配置映射初始化所有相机
     */
    int initializeCameras(const std::unordered_map<std::string, CameraConfig>& camera_configs);
    
    /**
     * @brief 启动指定相机（包括文件监听）
     */
    bool startCamera(const std::string& camera_id);
    
    /**
     * @brief 停止指定相机
     */
    void stopCamera(const std::string& camera_id);
    
    /**
     * @brief 启动所有相机
     */
    int startAllCameras();
    
    /**
     * @brief 停止所有相机
     */
    void stopAllCameras();
    
    /**
     * @brief 对指定相机的图像执行推理（手动推理）
     * @param camera_id 相机ID
     * @param image 输入图像
     * @return 推理结果列表
     */
    std::vector<nlohmann::json> inferImage(
        const std::string& camera_id,
        const cv::Mat& image
    );
    
    /**
     * @brief 设置推理结果回调函数
     */
    void setInferenceCallback(InferenceCallback callback);
    
    /**
     * @brief 获取相机实例
     */
    const CameraInstance* getCameraInstance(const std::string& camera_id) const;
    
    /**
     * @brief 检查相机是否存在
     */
    bool hasCamera(const std::string& camera_id) const;
    
    /**
     * @brief 获取活跃相机数量
     */
    int getActiveCameraCount() const;
    
    /**
     * @brief 获取所有相机ID列表
     */
    std::vector<std::string> getAllCameraIds() const;
    
private:
    /**
     * @brief 处理文件监听事件（内部回调）
     */
    void handleFileEvent(const std::string& camera_id, const FileWatcher::FileEvent& event);
    
    /**
     * @brief 验证相机配置的算法是否都已加载
     */
    bool validateCameraAlgorithms(const CameraConfig& config) const;
    
private:
    std::shared_ptr<InferenceEngine> inference_engine_;      
    std::unordered_map<std::string, CameraInstance> cameras_; 
    InferenceCallback inference_callback_;                  
    mutable std::mutex mutex_;                              
};

#endif // MULTI_CAMERA_MANAGER_HPP