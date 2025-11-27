#ifndef ALGORITHM_MANAGER_HPP
#define ALGORITHM_MANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "Algorithm.hpp"
#include "AlgorithmFactory.hpp"

struct AlgorithmConfig {
    std::string type;   
    std::string name;   
    std::string model_param; 
    std::string model_bin; 
    std::string label_path; 
    std::string preprocess; 
    std::string in_name; 
    std::string out_name;
    bool use_int8 = false; 
};

class AlgorithmManager {
public:
    AlgorithmManager();
    ~AlgorithmManager() = default;

    AlgorithmManager(const AlgorithmManager&) = delete;
    AlgorithmManager& operator=(const AlgorithmManager&) = delete;

    // 返回推理结果
    nlohmann::json infer(const std::string& algorithm_name, cv::Mat& image);
    
    std::string getAlgorithmType(const std::string& algorithm_name) const;
    bool hasAlgorithm(const std::string& algorithm_name) const;
    
    std::shared_ptr<algorithms::Algorithm> createNewInstance(const std::string& algorithm_name) const;
    
    const AlgorithmConfig* getAlgorithmConfig(const std::string& algorithm_name) const;

private:
    bool loadAlgorithmsConfig();
    bool loadAlgorithms();
    void parseAlgorithmConfigs(const nlohmann::json& json);

private:
    const std::string config_path_ = "./models/model.json";
    std::vector<AlgorithmConfig> algorithm_configs_;
    std::unordered_map<std::string, std::shared_ptr<algorithms::Algorithm>> algorithms_;
    std::unordered_map<std::string, std::string> algorithm_types_;  // 
    
    std::unordered_map<std::string, std::unique_ptr<std::mutex>> algorithm_mutexes_;
    mutable std::mutex map_mutex_;  
};

#endif // ALGORITHM_MANAGER_HPP
