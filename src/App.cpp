#include <iostream>
#include <sstream>      
#include <iomanip>     
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <mutex>
#include <future>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "common/MyController.hpp"
#include "algorithms/AlgorithmManager.hpp"
#include "camera/MultiCameraManager.hpp"
#include "inferEngine/InferenceEngine.hpp"
#include "inotify/FileWatcher.hpp"
#include "tools/Utils.hpp"
#include "tools/LazyFileSink.hpp"
#include "tools/MqttPublisher.hpp"
#include "api/HttpApiServer.hpp"
#include "api/InferenceSwitch.hpp"

static std::chrono::steady_clock::time_point g_last_pause_time;
static std::mutex g_pause_mutex;
static const int PAUSE_COOLDOWN_SECONDS = 5; 

// 全局 MQTT 发布器
static std::shared_ptr<MqttPublisher> g_mqtt_publisher; 

void handleInferenceResult(
    const std::string& camera_id,
    const std::string& algorithm_name,
    const cv::Mat& image,
    const nlohmann::json& result) {
    
    nlohmann::json output;
    output["camera_id"] = camera_id;
    output["algorithm"] = algorithm_name;
    output["result"] = result;
    
    spdlog::info("{}", output.dump()); 
    
    // 如果有检测结果，保存可视化图像
    if (result.contains("detections") && !result["detections"].empty()) {
        Utils::saveVisualization(image, result, camera_id, algorithm_name);

        
        if (g_mqtt_publisher) {
            g_mqtt_publisher->publishDefectDetected();
        }

        // 防止短时间重复发送暂停指令
        std::lock_guard<std::mutex> lock(g_pause_mutex);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_last_pause_time).count();
        
        if (elapsed >= PAUSE_COOLDOWN_SECONDS) {
            const std::string MOONRAKER_URL = "http://localhost:7125";
            
            static std::future<void> last_pause_future;
            last_pause_future = std::async(std::launch::async, [MOONRAKER_URL]() {
                bool success = Utils::pausePrinter(MOONRAKER_URL);
                if (success) {
                    spdlog::warn("Pause command sent successfully");
                }
            });
            
            g_last_pause_time = now;
            spdlog::info("Pause request dispatched (next pause available in {}s)", PAUSE_COOLDOWN_SECONDS);
        } else {
            spdlog::debug("Pause cooldown active, {}s remaining", PAUSE_COOLDOWN_SECONDS - elapsed);
        }
        
    }
}

void run(){
    spdlog::info(R"(
                                        |\      _,,,---,,_
                                ZZzz  /,`.-'`'    -.  ;-;;,_
                                    |,4-  ) )-,_. ,\ (  `'-'
                                    '---''(_/--'  `-'\_)
                                /\_/\     智能推理系统 v2.0
                                ( o.o )    开发者: Torch
                                > ^ <     
)");
    
    // 初始化 MQTT 发布器
    MqttPublisher::Config mqtt_config;
    mqtt_config.broker_host = "127.0.0.1";
    mqtt_config.broker_port = 1883;
    mqtt_config.client_id = "ncnn_inference_server";
    mqtt_config.topic = "opi/zero2/events/target_detected";
    mqtt_config.qos = 1;
    
    g_mqtt_publisher = std::make_shared<MqttPublisher>(mqtt_config);
    if (g_mqtt_publisher->connect()) {
        spdlog::info("✓ MQTT 发布器已连接到 {}:{}", mqtt_config.broker_host, mqtt_config.broker_port);
    } else {
        spdlog::warn("MQTT 发布器连接失败，检测事件将不会通过 MQTT 发送");
    }
    
    auto camera_configs = MyController::loadCameraConfigs("configs/config.json");
    if (camera_configs.empty()) {
        spdlog::error("未加载到任何相机配置");
        return;
    }
    
    std::set<std::string> required_algorithms;
    for (const auto& [camera_id, config] : camera_configs) {
        for (const auto& alg_name : config.algorithms) {
            required_algorithms.insert(alg_name);
        }
    }
    
    if (required_algorithms.empty()) {
        spdlog::error("没有相机配置算法");
        return;
    }
    
    auto algorithm_manager = std::make_shared<AlgorithmManager>();
    
    for (const auto& alg_name : required_algorithms) {
        if (!algorithm_manager->hasAlgorithm(alg_name)) {
            spdlog::error("算法 '{}' 未在 model.json 中定义", alg_name);
            return;
        }
    }
    
    auto inference_engine = std::make_shared<InferenceEngine>(algorithm_manager);
    
    std::map<std::string, std::vector<std::string>> camera_algorithm_mapping;
    for (const auto& [camera_id, config] : camera_configs) {
        camera_algorithm_mapping[camera_id] = config.algorithms;
    }
    inference_engine->updateMapping(camera_algorithm_mapping);
    
    auto multi_camera_manager = std::make_shared<MultiCameraManager>(inference_engine);
    
    multi_camera_manager->setInferenceCallback(
        [](const std::string& camera_id,
           const std::string& algorithm_name,
           const cv::Mat& image,
           const nlohmann::json& result) {
            handleInferenceResult(camera_id, algorithm_name, image, result);
        }
    );
    
    multi_camera_manager->initializeCameras(camera_configs);
    int started_count = multi_camera_manager->startAllCameras();
    
    if (started_count == 0) {
        spdlog::warn("没有相机配置文件监听，程序将退出");
        return;
    }
    
    // 启动 HTTP API 服务器（用于推理开关控制）
    auto http_server = std::make_unique<HttpApiServer>("0.0.0.0", 9090);
    if (http_server->start()) {
        spdlog::info("✓ HTTP API 服务器已启动");
        spdlog::info("  - POST http://localhost:9090/api/inference/toggle");
    } else {
        spdlog::warn("HTTP API 服务器启动失败");
    }
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        spdlog::debug("系统运行中，活跃相机: {}，推理状态: {}", 
                     multi_camera_manager->getActiveCameraCount(),
                     InferenceSwitch::getInstance().isEnabled() ? "enabled" : "disabled");
    }
}

int main(int argc, char** argv) {
    // 创建并配置控制台接收器
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    
    // 获取当前时间，用于生成日志文件名
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S");
    std::string filename = "logs/log_" + ss.str() + ".txt";
    
    // 创建延迟文件 sink（只在第一次 error 时创建文件）
    auto lazy_sink = std::make_shared<lazy_file_sink<std::mutex>>(filename);
    lazy_sink->set_level(spdlog::level::err);
    
    // 创建日志记录器
    auto logger = std::make_shared<spdlog::logger>("logger", spdlog::sinks_init_list{console_sink, lazy_sink});
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(3));
        
    // 运行主程序
    run();
    
    return 0;
}