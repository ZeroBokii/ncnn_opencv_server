#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <filesystem>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

class Utils {
public:
    static bool saveVisualization(
        const cv::Mat& image,
        const nlohmann::json& result,
        const std::string& camera_id,
        const std::string& algorithm_name,
        const std::string& output_base_dir = "output_results");
    
    static bool pausePrinter(const std::string& moonraker_url = "http://localhost:7125");
    
    /**
     * @brief 将浮点数格式化为保留两位小数
     * @param value 输入的浮点数
     * @return 保留两位小数的double值
     */
    static double roundToTwoDecimals(double value);

private:
    static void drawDetections(cv::Mat& image, const nlohmann::json& detections);
    
    static bool httpPost(const std::string& url);
};

#endif // UTILS_HPP