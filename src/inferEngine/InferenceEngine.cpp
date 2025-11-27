#include "InferenceEngine.hpp"

InferenceEngine::InferenceEngine(std::shared_ptr<AlgorithmManager> algorithm_mgr)
    : algorithm_manager_(std::move(algorithm_mgr))
    , exclusive_manager_(std::make_unique<ExclusiveInstanceManager>(algorithm_manager_)) {
    
    if (!algorithm_manager_) {
        throw std::runtime_error("AlgorithmManager cannot be null");
    }
}

InferenceEngine::~InferenceEngine() {
    spdlog::info("InferenceEngine destroyed");
}

bool InferenceEngine::isSharedAlgorithm(const std::string& algorithm_name) const {
    // 根据算法名称判断是否为共享实例算法
    // 约定：包含 "track" 或 "Track" 的算法为独占算法
    std::string lower_name = algorithm_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    return lower_name.find("track") == std::string::npos;
}

void InferenceEngine::updateMapping(
    const std::map<std::string, std::vector<std::string>>& new_mapping) {
    std::set<std::string> shared_algorithms_needed;
    std::vector<std::pair<std::string, std::string>> exclusive_instances_needed; // (camera_id, alg_name)
    
    for (const auto& [camera_id, algorithms] : new_mapping) {
        for (const auto& alg_name : algorithms) {
            if (!algorithm_manager_->hasAlgorithm(alg_name)) {
                spdlog::error("Algorithm '{}' not found in AlgorithmManager", alg_name);
                continue;
            }
            
            if (isSharedAlgorithm(alg_name)) {
                shared_algorithms_needed.insert(alg_name);
            } else {
                exclusive_instances_needed.emplace_back(camera_id, alg_name);
            }
        }
    }
    
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::shared_ptr<algorithms::Algorithm>>> 
        new_exclusive_instances;
    
    for (const auto& [camera_id, alg_name] : exclusive_instances_needed) {
        auto new_instance = algorithm_manager_->createNewInstance(alg_name);
        if (new_instance) {
            new_exclusive_instances[camera_id][alg_name] = new_instance;
            spdlog::info("✓ Pre-created exclusive instance '{}' for camera '{}'", 
                        alg_name, camera_id);
        } else {
            spdlog::error("✗ Failed to pre-create exclusive instance '{}' for camera '{}'",
                         alg_name, camera_id);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(mapping_mutex_);
        
        // 更新映射关系
        camera_algorithm_mapping_ = new_mapping;
        
        // 清除旧的实例池
        shared_pools_.clear();
        
        for (const auto& alg_name : shared_algorithms_needed) {
            shared_pools_[alg_name] = std::make_unique<ModelInstancePool>(
                alg_name, algorithm_manager_);
        }
        
        exclusive_manager_->clear();
        exclusive_manager_->replaceInstances(std::move(new_exclusive_instances));
    }
    
    spdlog::info("✓ Mapping updated: {} shared algorithms, {} exclusive instances",
                 shared_algorithms_needed.size(), exclusive_instances_needed.size());
}

std::vector<nlohmann::json> InferenceEngine::inferFrame(
    const std::string& camera_id, 
    const cv::Mat& frame) {
    
    if (frame.empty()) {
        spdlog::error("Empty frame for camera '{}'", camera_id);
        return {};
    }
    
    // 获取该相机的算法列表
    std::vector<std::string> algorithms;
    {
        std::lock_guard<std::mutex> lock(mapping_mutex_);
        auto it = camera_algorithm_mapping_.find(camera_id);
        if (it != camera_algorithm_mapping_.end()) {
            algorithms = it->second;
        }
    }
    
    if (algorithms.empty()) {
        spdlog::debug("No algorithms configured for camera '{}'", camera_id);
        return {};
    }
    
    // 执行推理
    std::vector<nlohmann::json> results;
    cv::Mat frame_copy = frame.clone();
    
    for (const auto& alg_name : algorithms) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            nlohmann::json result;
            
            // 根据算法类型选择使用共享或独占实例
            if (isSharedAlgorithm(alg_name)) {
                // 共享算法：使用 AlgorithmManager 的单一实例
                result = algorithm_manager_->infer(alg_name, frame_copy);
                spdlog::debug("[共享] Camera '{}' using shared instance of '{}'",
                             camera_id, alg_name);
            } else {
                auto exclusive_instance = exclusive_manager_->getInstance(camera_id, alg_name);
                if (exclusive_instance) {
                    result = exclusive_instance->infer(frame_copy);
                    spdlog::debug("[独占] Camera '{}' using exclusive instance of '{}'",
                                 camera_id, alg_name);
                } else {
                    spdlog::error("No exclusive instance found for camera '{}' algorithm '{}'",
                                 camera_id, alg_name);
                    continue;
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            if (!result.empty()) {
                {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    
                    cleanExpiredCache();
        
                    results_cache_[camera_id][alg_name] = {
                        result,
                        std::chrono::system_clock::now()
                    };
                }
                
                nlohmann::json alg_result;
                alg_result["camera_id"] = camera_id;
                alg_result["algorithm"] = alg_name;
                alg_result["inference_time_ms"] = duration.count();
                alg_result["result"] = result;
                results.push_back(alg_result);
                
                spdlog::debug("Camera '{}' algorithm '{}' inference completed in {} ms", 
                             camera_id, alg_name, duration.count());
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Inference error for camera '{}' algorithm '{}': {}", 
                         camera_id, alg_name, e.what());
        }
    }
    
    return results;
}

void InferenceEngine::cleanExpiredCache() {
    auto now = std::chrono::system_clock::now();
    int total_cleaned = 0;
    
    for (auto camera_it = results_cache_.begin(); camera_it != results_cache_.end(); ) {
        auto& cache = camera_it->second;
        
        for (auto cache_it = cache.begin(); cache_it != cache.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::hours>(
                now - cache_it->second.timestamp);
            
            if (age >= CACHE_EXPIRY_DURATION) {
                cache_it = cache.erase(cache_it);
                total_cleaned++;
            } else {
                ++cache_it;
            }
        }
        
        if (cache.empty()) {
            camera_it = results_cache_.erase(camera_it);
        } else {
            ++camera_it;
        }
    }
    
    if (total_cleaned > 0) {
        spdlog::debug("Cleaned {} expired cache entries (older than {} hour(s))", 
                     total_cleaned, CACHE_EXPIRY_DURATION.count());
    }
}