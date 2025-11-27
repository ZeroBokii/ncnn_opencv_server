#ifndef MODEL_INSTANCE_POOL_HPP
#define MODEL_INSTANCE_POOL_HPP

#include <memory>
#include <string>
#include <mutex>
#include <spdlog/spdlog.h>
#include "../algorithms/AlgorithmManager.hpp"

/**
 * @brief 模型实例池 - 为共享算法标记并发限制
 */
class ModelInstancePool {
public:
    /**
     * @brief 构造函数
     * @param algorithm_name 算法名称
     * @param algorithm_mgr 算法管理器
     * @param max_concurrent 最大并发数（默认 4）
     */
    ModelInstancePool(const std::string& algorithm_name, 
                     std::shared_ptr<AlgorithmManager> algorithm_mgr,
                     size_t max_concurrent = 4);

    const std::string& getAlgorithmName() const { return algorithm_name_; }
    
    size_t getMaxConcurrent() const { return max_concurrent_; }

private:
    std::string algorithm_name_;
    std::shared_ptr<AlgorithmManager> algorithm_manager_;
    const size_t max_concurrent_;
};

#endif // MODEL_INSTANCE_POOL_HPP