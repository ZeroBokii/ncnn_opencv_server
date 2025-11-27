#include "ModelInstancePool.hpp"

ModelInstancePool::ModelInstancePool(const std::string& algorithm_name, 
                                   std::shared_ptr<AlgorithmManager> algorithm_mgr,
                                   size_t max_concurrent)
    : algorithm_name_(algorithm_name)
    , algorithm_manager_(algorithm_mgr)
    , max_concurrent_(max_concurrent) {
    
    if (!algorithm_manager_) {
        throw std::runtime_error("AlgorithmManager cannot be null");
    }
    
    if (!algorithm_manager_->hasAlgorithm(algorithm_name_)) {
        throw std::runtime_error("Algorithm '" + algorithm_name_ + "' not found in AlgorithmManager");
    }
}