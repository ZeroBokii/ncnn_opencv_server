#include "AlgorithmManager.hpp"

AlgorithmManager::AlgorithmManager() {
    if (!loadAlgorithmsConfig()) {
        spdlog::error("Failed to load algorithms config from: {}", config_path_);
        return;
    }

    if (!loadAlgorithms()) {
        spdlog::error("Failed to initialize algorithms");
        return;
    }

}

bool AlgorithmManager::loadAlgorithmsConfig() {
    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            spdlog::error("Cannot open config file: {}", config_path_);
            return false;
        }

        nlohmann::json json_config = nlohmann::json::parse(file);
        parseAlgorithmConfigs(json_config);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing config file: {}", e.what());
        return false;
    }
}

void AlgorithmManager::parseAlgorithmConfigs(const nlohmann::json& json_config) {
    if (!json_config.contains("models") || !json_config["models"].is_array()) {
        spdlog::error("Invalid config format: missing or invalid 'models' array");
        return;
    }

    algorithm_configs_.clear();
    algorithm_configs_.reserve(json_config["models"].size());
    algorithm_types_.clear();

    for (const auto& model : json_config["models"]) {
        try {
            AlgorithmConfig config{
                model["type"].get<std::string>(),
                model["name"].get<std::string>(),
                model["model_param"].get<std::string>(),
                model["model_bin"].get<std::string>(),
                model["label_path"].get<std::string>(),
                model["preprocess"].get<std::string>(),
                model["in_name"].get<std::string>(),
                model["out_name"].get<std::string>(),
                model.value("int8", false)
            };
            algorithm_configs_.push_back(config);
            
            algorithm_types_[config.name] = config.type;
            
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse model config: {}", e.what());
            continue;
        }
    }
}

bool AlgorithmManager::loadAlgorithms() {
    algorithms_.clear();
    algorithms_.reserve(algorithm_configs_.size());
    algorithm_mutexes_.clear();

    for (const auto& config : algorithm_configs_) {
        try {
            auto algorithm = AlgorithmFactory::createAlgorithm(
                config.type,
                config.name,
                config.model_param,
                config.model_bin,
                config.label_path,
                config.preprocess,
                config.in_name,
                config.out_name,
                config.use_int8
            );

            if (algorithm) {
                algorithms_.emplace(config.name, std::move(algorithm));
                // 为每个算法创建独立的mutex
                algorithm_mutexes_.emplace(config.name, std::make_unique<std::mutex>());
                spdlog::info("Loaded algorithm: {} ({})", config.name, config.type);
            } else {
                spdlog::error("Failed to create algorithm: {}", config.name);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error loading algorithm {}: {}", config.name, e.what());
            continue;
        }
    }

    return !algorithms_.empty();
}

nlohmann::json AlgorithmManager::infer(
    const std::string& algorithm_name, 
    cv::Mat& image) {
    
    std::shared_ptr<algorithms::Algorithm> algorithm;
    std::mutex* alg_mutex = nullptr;
    {
        std::lock_guard<std::mutex> lock(map_mutex_);  
        auto alg_it = algorithms_.find(algorithm_name);
        if (alg_it == algorithms_.end()) {
            spdlog::error("Algorithm not found: {}", algorithm_name);
            return nlohmann::json();
        }
        algorithm = alg_it->second;
        
        auto mutex_it = algorithm_mutexes_.find(algorithm_name);
        if (mutex_it == algorithm_mutexes_.end()) {
            spdlog::error("Algorithm mutex not found: {}", algorithm_name);
            return nlohmann::json();
        }
        alg_mutex = mutex_it->second.get();
    }

    std::lock_guard<std::mutex> alg_lock(*alg_mutex);
    
    try {
        nlohmann::json result = algorithm->infer(image);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Inference error for {}: {}", algorithm_name, e.what());
        return nlohmann::json();
    }
}

bool AlgorithmManager::hasAlgorithm(const std::string& algorithm_name) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    return algorithms_.find(algorithm_name) != algorithms_.end();
}

std::string AlgorithmManager::getAlgorithmType(const std::string& algorithm_name) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = algorithm_types_.find(algorithm_name);
    return it != algorithm_types_.end() ? it->second : "";
}

const AlgorithmConfig* AlgorithmManager::getAlgorithmConfig(const std::string& algorithm_name) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    for (const auto& config : algorithm_configs_) {
        if (config.name == algorithm_name) {
            return &config;
        }
    }
    return nullptr;
}

std::shared_ptr<algorithms::Algorithm> AlgorithmManager::createNewInstance(
    const std::string& algorithm_name) const {
    
    const AlgorithmConfig* config = getAlgorithmConfig(algorithm_name);
    if (!config) {
        spdlog::error("Algorithm config not found: {}", algorithm_name);
        return nullptr;
    }
    
    try {
        auto algorithm = AlgorithmFactory::createAlgorithm(
            config->type,
            config->name,
            config->model_param,
            config->model_bin,
            config->label_path,
            config->preprocess,
            config->in_name,
            config->out_name,
            config->use_int8 
        );
        
        if (algorithm) {
            spdlog::info("Created new instance for algorithm: {} ({})", 
                        config->name, config->type);
        } else {
            spdlog::error("Failed to create new instance: {}", algorithm_name);
        }
        
        return algorithm;
        
    } catch (const std::exception& e) {
        spdlog::error("Error creating new instance for {}: {}", algorithm_name, e.what());
        return nullptr;
    }
}