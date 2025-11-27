//
// Created for ncnn_demo - Camera Manager
//

#ifndef CAMERA_MANAGER_HPP
#define CAMERA_MANAGER_HPP

#include <string>
#include <vector>

/**
 * @brief 相机配置结构
 */
struct CameraConfig {
    std::string camera;              // 相机id（camera_top, camera_left等）
    std::vector<std::string> algorithms; // 该相机使用的算法列表
    std::string watch_path;              // 监听路径（可选）
    bool has_watcher;                    // 是否配置了监听

    bool operator!=(const CameraConfig& other) const {
        return camera != other.camera ||
               algorithms != other.algorithms ||
               watch_path != other.watch_path ||
               has_watcher != other.has_watcher;
    }
};

#endif //CAMERA_MANAGER_HPP