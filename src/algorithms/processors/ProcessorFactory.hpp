#ifndef PROCESSOR_FACTORY_HPP
#define PROCESSOR_FACTORY_HPP

#include "ProcessorBase.hpp"
#include "YOLO/Yolo5.hpp"
#include "YOLO/Yolo11.hpp"
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace algorithms {
namespace processors {

class PreprocessorFactory {
public:
    static std::shared_ptr<Preprocessor> create(
        const std::string& preprocess_type,
        int input_width = 640,
        int input_height = 640) {
        
        if (preprocess_type == "yolo" || preprocess_type == "yolov5" || 
            preprocess_type == "yolo11") {
            return std::make_shared<Yolov5Preprocessor>(input_width, input_height);
        }
        return std::make_shared<Yolov5Preprocessor>(input_width, input_height);
    }
};

class PostprocessorFactory {
public:
    static std::shared_ptr<Postprocessor> create(
        const std::string& preprocess_type,
        float conf_threshold = 0.5f,
        float nms_threshold = 0.45f) {
        
        if (preprocess_type == "yolo11") {
            return std::make_shared<Yolo11Postprocessor>(conf_threshold, nms_threshold);
        }
        
        if (preprocess_type == "yolo" || preprocess_type == "yolov5") {
            return std::make_shared<Yolov5Postprocessor>(conf_threshold, nms_threshold);
        }
        
        return std::make_shared<Yolov5Postprocessor>(conf_threshold, nms_threshold);
    }
};

} // namespace processors
} // namespace algorithms

#endif // PROCESSOR_FACTORY_HPP