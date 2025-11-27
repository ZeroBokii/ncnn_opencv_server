#ifndef ALGORITHM_FACTORY_HPP
#define ALGORITHM_FACTORY_HPP

#include <memory>
#include <string>
#include "DetectionAlgorithm.hpp"

class AlgorithmFactory {
public:
    static std::shared_ptr<algorithms::Algorithm> createAlgorithm(
        const std::string& type,
        const std::string& name,
        const std::string& model_param,
        const std::string& model_bin,
        const std::string& labelPath,
        const std::string& preprocess,
        const std::string& in_name,
        const std::string& out_name,
        bool use_int8 = false) {  // 添加 use_int8 参数，默认为 false 保持向后兼容
            
        if (type == "Detection") {
            return std::make_shared<DetectionAlgorithm>(name, model_param, model_bin, 
                labelPath, preprocess, in_name, out_name, use_int8);
        } 
        
        throw std::runtime_error("Unknown algorithm type: " + type);
    }
};

#endif // ALGORITHM_FACTORY_HPP
