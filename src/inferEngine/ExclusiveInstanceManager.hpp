#ifndef EXCLUSIVE_INSTANCE_MANAGER_HPP
#define EXCLUSIVE_INSTANCE_MANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <spdlog/spdlog.h>
#include "../algorithms/AlgorithmManager.hpp"
#include "../algorithms/Algorithm.hpp"

/**
 * @brief 独占实例管理器 - 为每个相机创建独立的算法实例
 */
class ExclusiveInstanceManager {
public:
    ExclusiveInstanceManager(std::shared_ptr<AlgorithmManager> algorithm_mgr)
        : algorithm_manager_(algorithm_mgr) {}
    
    /**
     * @brief 为相机创建独占算法实例
     */
    bool addInstance(const std::string& camera_id, 
                    const std::string& algorithm_name);

    /**
     * @brief 获取相机的独占算法实例
     * @param camera_id 相机id
     * @param algorithm_name 算法名称
     * @return 算法实例指针，如果不存在返回 nullptr
     */
    std::shared_ptr<algorithms::Algorithm> getInstance(
        const std::string& camera_id,
        const std::string& algorithm_name) const;
    
    bool hasInstance(const std::string& camera_id, 
                    const std::string& algorithm_name) const;
    
    void removeInstance(const std::string& camera_id);
    
    void clear();
    
    /**
     * @brief 替换所有独占实例
     */
    void replaceInstances(
        std::unordered_map<std::string, 
            std::unordered_map<std::string, std::shared_ptr<algorithms::Algorithm>>> new_instances);

private:
    std::shared_ptr<AlgorithmManager> algorithm_manager_;
    
    // camera_id -> (algorithm_name -> algorithm_instance)
    std::unordered_map<std::string, 
        std::unordered_map<std::string, std::shared_ptr<algorithms::Algorithm>>> 
        exclusive_instances_;
    
    mutable std::mutex mutex_;
};

#endif // EXCLUSIVE_INSTANCE_MANAGER_HPP