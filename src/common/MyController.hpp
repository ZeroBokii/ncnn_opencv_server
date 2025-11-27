//
// Created by ubuntu on 25-11-12.
//

#ifndef MYCONTROLLER_HPP
#define MYCONTROLLER_HPP

#include <fstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>

#include "../camera/CameraManager.hpp"

class MyController {
public:    
    MyController() {}
    
    /**
     * @brief 从 JSON 文件加载所有相机配置
     * @param filename 配置文件路径
     * @return 相机配置映射（camera_id -> CameraConfig）
     */
    static std::unordered_map<std::string, CameraConfig> loadCameraConfigs(const std::string &filename);
};

#endif //MYCONTROLLER_HPP
