#include "Utils.hpp"
#include <curl/curl.h>
#include <cmath>

bool Utils::saveVisualization(
    const cv::Mat& image,
    const nlohmann::json& result,
    const std::string& camera_id,
    const std::string& algorithm_name,
    const std::string& output_base_dir) {
     
    if (image.empty()) {
        spdlog::warn("saveVisualization: image is empty");
        return false;
    }
    
    // 检查是否有检测结果
    if (!result.contains("detections") || !result["detections"].is_array()) {
        spdlog::debug("saveVisualization: no detections found");
        return false;
    }
    
    if (result["detections"].empty()) {
        spdlog::debug("saveVisualization: zero detections");
        return false;
    }
    
    try {
        // 克隆图像以避免修改原图
        cv::Mat vis_image = image.clone();
        
        // 绘制检测框
        drawDetections(vis_image, result["detections"]);
        
        // 创建相机专用输出目录
        std::string output_dir = output_base_dir + "/" + camera_id;
        std::filesystem::create_directories(output_dir);
        
        // 生成时间戳文件名
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::string output_path = output_dir + "/" + algorithm_name + "_" + 
                                 std::to_string(timestamp) + ".jpg";
        
        // 保存图像
        bool success = cv::imwrite(output_path, vis_image);
        if (success) {
            spdlog::debug("  保存结果: {}", output_path);
        } else {
            spdlog::error("  保存失败: {}", output_path);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        spdlog::error("saveVisualization: error - {}", e.what());
        return false;
    }
}

void Utils::drawDetections(cv::Mat& image, const nlohmann::json& detections) {
    if (!detections.is_array()) {
        spdlog::warn("drawDetections: detections is not an array");
        return;
    }
    
    for (const auto& detection : detections) {
        try {
            std::string label = detection.value("label", "unknown");
            float score = detection.value("score", 0.0f);
            int cls_id = detection.value("cls_id", 0);
            
            auto box = detection["box"];
            float left = box.value("left", 0.0f);
            float top = box.value("top", 0.0f);
            float right = box.value("right", 0.0f);
            float bottom = box.value("bottom", 0.0f);
            
            // 根据类别ID生成固定颜色（使用黄金角分割）
            int hue = (cls_id * 137) % 360;
            cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue / 2, 255, 255));
            cv::Mat bgr;
            cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
            cv::Vec3b bgr_pixel = bgr.at<cv::Vec3b>(0, 0);
            cv::Scalar color(bgr_pixel[0], bgr_pixel[1], bgr_pixel[2]);
            
            // 绘制检测框
            cv::rectangle(image,
                        cv::Point(static_cast<int>(left), static_cast<int>(top)),
                        cv::Point(static_cast<int>(right), static_cast<int>(bottom)),
                        color, 2, cv::LINE_AA);
            
            // 格式化标签文本（包含置信度）
            std::string display_text = label + " " + cv::format("%.2f", score);
            int baseline = 0;
            cv::Size label_size = cv::getTextSize(display_text,
                                                 cv::FONT_HERSHEY_SIMPLEX,
                                                 0.6, 1, &baseline);
            
            // 确保标签框不超出图像边界
            int label_top = std::max(static_cast<int>(top), label_size.height);
            
            // 绘制标签背景框
            cv::rectangle(image,
                        cv::Point(static_cast<int>(left), label_top - label_size.height),
                        cv::Point(static_cast<int>(left) + label_size.width, label_top + baseline),
                        color, -1);
            
            // 绘制标签文本（白色）
            cv::putText(image, display_text,
                      cv::Point(static_cast<int>(left), label_top),
                      cv::FONT_HERSHEY_SIMPLEX, 0.6,
                      cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
                      
        } catch (const std::exception& e) {
            spdlog::error("drawDetections: error processing detection - {}", e.what());
            continue;
        }
    }
}

double Utils::roundToTwoDecimals(double value) {
    return std::floor(value * 100.0 + 0.5) / 100.0;
}

bool Utils::pausePrinter(const std::string& moonraker_url) {
    std::string url = moonraker_url + "/printer/print/pause";
    bool success = httpPost(url);
    
    if (success) {
        spdlog::warn(" [PRINTER] Printing PAUSED via Moonraker");
    } else {
        spdlog::error(" [PRINTER] Failed to pause printing");
    }
    
    return success;
}

bool Utils::httpPost(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        spdlog::error("Failed to initialize CURL");
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        spdlog::error("HTTP POST failed: {} ({})", curl_easy_strerror(res), url);
        return false;
    }
    
    if (http_code >= 200 && http_code < 300) {
        spdlog::debug("HTTP POST {} - Status: {}", url, http_code);
        return true;
    } else {
        spdlog::error("HTTP POST {} failed with status {}", url, http_code);
        return false;
    }
}