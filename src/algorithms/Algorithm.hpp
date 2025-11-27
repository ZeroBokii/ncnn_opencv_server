#ifndef ALGORITHM_HPP
#define ALGORITHM_HPP

#include <random>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

namespace algorithms {
    class Algorithm {
public:
    virtual ~Algorithm() = default;

    // 接口：推理函数
    virtual nlohmann::json infer(cv::Mat &image) = 0;

    // 接口：获取算法名称
    virtual std::string getName() const = 0;
    
protected:
    void generateLabelColorPairs(const std::string& labelFile) {
        std::ifstream file(labelFile);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open labels file: " + labelFile);
        }
        
        m_labels.clear();
        std::string line;
        int index = 0;
        
        while (std::getline(file, line)) {
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            
            if (!line.empty()) {
                int hue = (index * 137) % 360;
                cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue / 2, 255, 255));
                cv::Mat bgr;
                cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
                cv::Vec3b bgr_pixel = bgr.at<cv::Vec3b>(0, 0);
                cv::Scalar color(bgr_pixel[0], bgr_pixel[1], bgr_pixel[2]);
                
                m_labels.emplace_back(line, color);
                index++;
            }
        }
        
        file.close();
        
        if (m_labels.empty()) {
            throw std::runtime_error("No labels loaded from file: " + labelFile);
        }
    }

    std::vector<std::pair<std::string, cv::Scalar>> m_labels;
    std::string name_;
};
}

#endif // ALGORITHM_HPP
