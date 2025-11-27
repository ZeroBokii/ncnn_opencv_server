#ifndef YOLO_HPP
#define YOLO_HPP

#include "../ProcessorBase.hpp"
#include <vector>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace algorithms {
namespace processors {

class Yolov5Preprocessor  : public Preprocessor {
public:
    explicit Yolov5Preprocessor (int target_size = 640)
        : target_width_(target_size), target_height_(target_size) { 
        spdlog::debug("Yolov5Preprocessor  created: target_size={}x{}", target_width_, target_height_);
    }
    Yolov5Preprocessor (int target_width, int target_height)
        : target_width_(target_width), target_height_(target_height) {
        spdlog::debug("Yolov5Preprocessor  created: target_size={}x{}", target_width_, target_height_);
    }
 
    PreprocessResult process(const cv::Mat& image) override {
        PreprocessResult result;
        
        if (image.empty()) {
            spdlog::error("Yolov5Preprocessor : input image is empty");
            return result;
        }
        
        result.original_width = image.cols;
        result.original_height = image.rows;
        
        int new_w, new_h;
        calculateLetterboxParams(image.cols, image.rows,
                                result.scale, new_w, new_h,
                                result.wpad, result.hpad);
        
        ncnn::Mat resized = ncnn::Mat::from_pixels_resize(
            image.data, ncnn::Mat::PIXEL_BGR2RGB,
            image.cols, image.rows, new_w, new_h);
        
        ncnn::copy_make_border(resized, result.preprocessed_mat,
                              result.hpad / 2, result.hpad - result.hpad / 2,
                              result.wpad / 2, result.wpad - result.wpad / 2,
                              ncnn::BORDER_CONSTANT, 114.f);
        
        const float norm_vals[3] = {1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f};
        result.preprocessed_mat.substract_mean_normalize(nullptr, norm_vals);
        
        return result;
    }
    
    std::string getName() const override {
        return "yolo";
    }
    
private:
    void calculateLetterboxParams(int orig_w, int orig_h,
                            float& scale, int& new_w, int& new_h,
                            int& wpad, int& hpad) {
        float scale_w = static_cast<float>(target_width_) / orig_w;
        float scale_h = static_cast<float>(target_height_) / orig_h;
        scale = std::min(scale_w, scale_h);
        
        new_w = static_cast<int>(orig_w * scale);
        new_h = static_cast<int>(orig_h * scale);
        
        wpad = target_width_ - new_w;
        hpad = target_height_ - new_h;
    }

    int target_width_;
    int target_height_;
};

class Yolov5Postprocessor  : public Postprocessor {
public:
    explicit Yolov5Postprocessor (float conf_threshold = 0.5f, float nms_threshold = 0.45f)
        : conf_threshold_(conf_threshold), nms_threshold_(nms_threshold) {
        spdlog::debug("Yolov5Postprocessor  created: conf={}, nms={}",
                     conf_threshold_, nms_threshold_);
    }
    
    Result process(const ncnn::Mat& output, 
              const PreprocessResult& preprocess_result) override {
        Result result;
        
        if (output.empty()) {
            spdlog::warn("Yolov5Postprocessor : empty output tensor");
            return result;
        }
        
        int num_boxes = output.h;
        int box_dim = output.w;
        
        if (box_dim < 5) {
            spdlog::error("Invalid output dimension: {}", box_dim);
            return result;
        }
        
        int num_classes = box_dim - 5;
        const float* data_ptr = (const float*)output.data;
        
        std::vector<Detection> detections;
        
        for (int i = 0; i < num_boxes; i++) {
            const float* box_data = data_ptr + i * box_dim;
            
            float x_center = box_data[0];
            float y_center = box_data[1];
            float width = box_data[2];
            float height = box_data[3];
            float objectness = box_data[4];
            
            float max_class_score = 0.0f;
            int max_class_id = 0;
            for (int c = 0; c < num_classes; c++) {
                float class_score = box_data[5 + c];
                if (class_score > max_class_score) {
                    max_class_score = class_score;
                    max_class_id = c;
                }
            }
            
            float confidence = objectness * max_class_score;
            
            if (confidence > conf_threshold_) {
                Detection det;
                det.x = x_center - width / 2.0f;
                det.y = y_center - height / 2.0f;
                det.width = width;
                det.height = height;
                det.confidence = confidence;
                det.class_id = max_class_id;
                detections.push_back(det);
            }
        }
        
        std::vector<Detection> final_detections = applyNMS(detections);
        
        result.boxes.reserve(final_detections.size());
        for (const auto& det : final_detections) {
            float x_orig = (det.x - preprocess_result.wpad / 2.0f) / preprocess_result.scale;
            float y_orig = (det.y - preprocess_result.hpad / 2.0f) / preprocess_result.scale;
            float w_orig = det.width / preprocess_result.scale;
            float h_orig = det.height / preprocess_result.scale;
            
            x_orig = std::max(0.0f, std::min(x_orig, static_cast<float>(preprocess_result.original_width)));
            y_orig = std::max(0.0f, std::min(y_orig, static_cast<float>(preprocess_result.original_height)));
            w_orig = std::min(w_orig, preprocess_result.original_width - x_orig);
            h_orig = std::min(h_orig, preprocess_result.original_height - y_orig);
            
            Box box;
            box.left = x_orig;
            box.top = y_orig;
            box.right = x_orig + w_orig;
            box.bottom = y_orig + h_orig;
            box.cls_id = det.class_id;
            box.score = det.confidence;
            
            result.boxes.push_back(box);
        }
        
        return result;
    }
    
    std::string getName() const override {
        return "yolo";
    }
    
private:
    struct Detection {
        float x, y, width, height;
        float confidence;
        int class_id;
    };
    
    float calculateIoU(const Detection& a, const Detection& b) const {
        float x1 = std::max(a.x, b.x);
        float y1 = std::max(a.y, b.y);
        float x2 = std::min(a.x + a.width, b.x + b.width);
        float y2 = std::min(a.y + a.height, b.y + b.height);
        
        float inter_w = std::max(0.0f, x2 - x1);
        float inter_h = std::max(0.0f, y2 - y1);
        float inter_area = inter_w * inter_h;
        
        float area_a = a.width * a.height;
        float area_b = b.width * b.height;
        float union_area = area_a + area_b - inter_area;
        
        return union_area > 0.0f ? inter_area / union_area : 0.0f;
    }
    
    std::vector<Detection> applyNMS(std::vector<Detection>& detections) const {
        if (detections.empty()) {
            return detections;
        }
        
        std::sort(detections.begin(), detections.end(),
                 [](const Detection& a, const Detection& b) {
                     return a.confidence > b.confidence;
                 });
        
        std::vector<bool> suppressed(detections.size(), false);
        std::vector<Detection> result;
        
        for (size_t i = 0; i < detections.size(); i++) {
            if (suppressed[i]) continue;
            
            result.push_back(detections[i]);
            
            for (size_t j = i + 1; j < detections.size(); j++) {
                if (suppressed[j]) continue;
                
                if (detections[i].class_id == detections[j].class_id) {
                    float iou = calculateIoU(detections[i], detections[j]);
                    if (iou > nms_threshold_) {
                        suppressed[j] = true;
                    }
                }
            }
        }
        
        return result;
    }
    
    float conf_threshold_;
    float nms_threshold_;
};

} // namespace processors
} // namespace algorithms

#endif // YOLO_HPP