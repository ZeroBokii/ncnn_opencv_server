#include "ExclusiveInstanceManager.hpp"

bool ExclusiveInstanceManager::addInstance(
    const std::string& camera_id,
    const std::string& algorithm_name) {
    
    if (!algorithm_manager_) {
        spdlog::error("AlgorithmManager is null");
        return false;
    }
    
    // 为相机创建新的算法实例
    auto new_instance = algorithm_manager_->createNewInstance(algorithm_name);
    if (!new_instance) {
        spdlog::error("Failed to create exclusive instance '{}' for camera '{}'",
                     algorithm_name, camera_id);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    exclusive_instances_[camera_id][algorithm_name] = new_instance;

    return true;
}

std::shared_ptr<algorithms::Algorithm> ExclusiveInstanceManager::getInstance(
    const std::string& camera_id,
    const std::string& algorithm_name) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto camera_it = exclusive_instances_.find(camera_id);
    if (camera_it == exclusive_instances_.end()) {
        return nullptr;
    }
    
    auto alg_it = camera_it->second.find(algorithm_name);
    if (alg_it == camera_it->second.end()) {
        return nullptr;
    }
    
    return alg_it->second;
}

bool ExclusiveInstanceManager::hasInstance(
    const std::string& camera_id,
    const std::string& algorithm_name) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto camera_it = exclusive_instances_.find(camera_id);
    if (camera_it == exclusive_instances_.end()) {
        return false;
    }
    
    return camera_it->second.find(algorithm_name) != camera_it->second.end();
}

void ExclusiveInstanceManager::removeInstance(const std::string& camera_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exclusive_instances_.find(camera_id);
    if (it != exclusive_instances_.end()) {
        int count = it->second.size();
        exclusive_instances_.erase(it);
        spdlog::info("Removed {} exclusive instances for camera '{}'", count, camera_id);
    }
}

void ExclusiveInstanceManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int total_count = 0;
    for (const auto& [camera_id, algorithms] : exclusive_instances_) {
        total_count += algorithms.size();
    }
    
    exclusive_instances_.clear();
    
    if (total_count > 0) {
        spdlog::info("Cleared {} exclusive instances across all cameras", total_count);
    }
}

void ExclusiveInstanceManager::replaceInstances(
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::shared_ptr<algorithms::Algorithm>>> new_instances) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int old_count = 0;
    for (const auto& [camera_id, algorithms] : exclusive_instances_) {
        old_count += algorithms.size();
    }
    
    int new_count = 0;
    for (const auto& [camera_id, algorithms] : new_instances) {
        new_count += algorithms.size();
    }
    
    exclusive_instances_ = std::move(new_instances);
}