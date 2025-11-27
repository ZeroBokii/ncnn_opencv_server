#include "MyController.hpp"

/**
 * @brief 从 JSON 文件加载所有相机配置
 *
 * @param filename 配置文件路径
 * @return std::unordered_map<std::string, CameraConfig> 相机配置映射
 */
std::unordered_map<std::string, CameraConfig> MyController::loadCameraConfigs(const std::string &filename) {
    std::unordered_map<std::string, CameraConfig> camera_configs;
    
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("无法打开配置文件: {}", filename);
        return camera_configs;
    }
    
    const auto fileSize = file.tellg();
    file.seekg(0);
    std::string jsonStr(fileSize, '\0');
    file.read(jsonStr.data(), fileSize);
    file.close();
    
    try {
        const auto j = nlohmann::json::parse(jsonStr);
        
        if (!j.contains("config_mapping")) {
            spdlog::error("JSON 配置中缺少 'config_mapping' 字段");
            return camera_configs;
        }
        
        const auto& config_mapping = j["config_mapping"];
        
        for (auto it = config_mapping.begin(); it != config_mapping.end(); ++it) {
            CameraConfig config;
            config.camera = it.key();
            config.has_watcher = false;
            
            const auto& camera_data = it.value();
            
            if (camera_data.contains("alg") && camera_data["alg"].is_array()) {
                for (const auto& alg : camera_data["alg"]) {
                    config.algorithms.push_back(alg.get<std::string>());
                }
            }
            
            if (camera_data.contains("watcher")) {
                config.watch_path = camera_data["watcher"].get<std::string>();
                config.has_watcher = !config.watch_path.empty();
            }
            
            camera_configs[config.camera] = config;
            
            spdlog::info("✓ 加载相机配置: {}", config.camera);
            spdlog::info("  - 算法: {}", nlohmann::json(config.algorithms).dump());
            if (config.has_watcher) {
                spdlog::info("  - 监听路径: {}", config.watch_path);
            }
        }
        
        spdlog::info("✓ 总计加载 {} 个相机配置", camera_configs.size());
        
    } catch (const nlohmann::json::parse_error &e) {
        spdlog::error("配置 JSON 格式有误: {}", e.what());
    } catch (const std::exception &e) {
        spdlog::error("加载相机配置时发生异常: {}", e.what());
    }
    
    return camera_configs;
}