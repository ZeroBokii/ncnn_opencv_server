#ifndef INFERENCE_ENGINE_HPP
#define INFERENCE_ENGINE_HPP

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "../camera/CameraManager.hpp"
#include "../algorithms/AlgorithmManager.hpp"
#include "ModelInstancePool.hpp"
#include "ExclusiveInstanceManager.hpp"

/**
 * @brief 推理引擎 - 协调多相机多算法推理
 */
class InferenceEngine {
public:
    InferenceEngine(std::shared_ptr<AlgorithmManager> algorithm_mgr);
    ~InferenceEngine();

    /**
     * @brief 更新相机到算法的映射关系
     * @param new_mapping camera_id -> [algorithm_names]
     */
    void updateMapping(const std::map<std::string, std::vector<std::string>>& new_mapping);
    
    /**
     * @brief 对指定相机的图像执行推理
     * @param camera_id 相机ID
     * @param frame 输入图像
     */
    std::vector<nlohmann::json> inferFrame(const std::string& camera_id, const cv::Mat& frame);
    
    /**
     * @brief 检查算法是否为共享算法
     */
    bool isSharedAlgorithm(const std::string& algorithm_name) const;

private:
    struct InferenceResult {
        nlohmann::json result;
        std::chrono::system_clock::time_point timestamp;
    };
    using ResultCache = std::unordered_map<std::string, InferenceResult>;
    
    // 缓存过期时间（1小时）
    static constexpr std::chrono::hours CACHE_EXPIRY_DURATION{1};
    
    /**
     * @brief 清理过期的缓存条目
     * @note 调用此方法前必须持有 cache_mutex_
     */
    void cleanExpiredCache();

    // 基础组件
    std::shared_ptr<AlgorithmManager> algorithm_manager_;
    
    // 算法实例管理
    std::unordered_map<std::string, std::unique_ptr<ModelInstancePool>> shared_pools_;
    std::unique_ptr<ExclusiveInstanceManager> exclusive_manager_;
    
    // 状态管理
    std::mutex mapping_mutex_;
    std::map<std::string, std::vector<std::string>> camera_algorithm_mapping_;
    
    // 结果缓存
    std::mutex cache_mutex_;
    std::unordered_map<std::string, ResultCache> results_cache_;
};

#endif // INFERENCE_ENGINE_HPP