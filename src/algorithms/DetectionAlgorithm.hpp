#ifndef DETECTION_ALGORITHM_HPP
#define DETECTION_ALGORITHM_HPP
#include "Algorithm.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include "../deploy/NcnnInference.hpp"
#include "processors/ProcessorFactory.hpp"
#include "../tools/Utils.hpp"

class DetectionAlgorithm : public algorithms::Algorithm{
public:
    explicit DetectionAlgorithm(const std::string &name, const std::string &model_param,
                               const std::string &model_bin, const std::string &labelPath,
                               const std::string &preprocess, const std::string &in_name,
                               const std::string &out_name, bool use_int8 = false)
        : detector_(std::make_shared<NcnnEngine::NcnnInference>(model_param, model_bin, in_name, out_name)) {
            
            if (!detector_) {
                throw std::runtime_error("DetectionAlgorithm: Failed to create NcnnInference instance for '" + name + "'");
            }
            
            detector_->setUseInt8(use_int8);
            
            bool init_success = detector_->initialize();
            if (!init_success) {
                throw std::runtime_error("DetectionAlgorithm: NCNN model initialization failed for '" + name + "' (param: " + model_param + ")");
            }
            
            try {
                preprocessor_ = algorithms::processors::PreprocessorFactory::create(preprocess);
                if (!preprocessor_) {
                    throw std::runtime_error("Preprocessor creation returned nullptr");
                }
            } catch (const std::exception& e) {
                throw std::runtime_error("DetectionAlgorithm: Failed to create preprocessor for '" + name + "': " + e.what());
            }
            
            try {
                postprocessor_ = algorithms::processors::PostprocessorFactory::create(preprocess, 0.8f, 0.45f);
                if (!postprocessor_) {
                    throw std::runtime_error("Postprocessor creation returned nullptr");
                }
            } catch (const std::exception& e) {
                throw std::runtime_error("DetectionAlgorithm: Failed to create postprocessor for '" + name + "': " + e.what());
            }
            
            try {
                generateLabelColorPairs(labelPath);
            } catch (const std::exception& e) {
                throw std::runtime_error("DetectionAlgorithm: Failed to load labels from '" + labelPath + "' for '" + name + "': " + e.what());
            }
            
            name_ = name;
            
            spdlog::info("✓ DetectionAlgorithm '{}' initialized successfully", name_);
        }

    nlohmann::json infer(cv::Mat &image) override {
        nlohmann::json output;
        if (image.empty()) {
            spdlog::error("DetectionAlgorithm: input image is empty");
            return output;
    }

        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            algorithms::processors::PreprocessResult preprocess_result = preprocessor_->process(image);

            ncnn::Mat inference_output = detector_->infer(preprocess_result.preprocessed_mat);
            
            algorithms::processors::Result result = postprocessor_->process(inference_output, preprocess_result);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            output["time_ms"] = duration;
            output["detections"] = nlohmann::json::array();
            
            for (const auto& box : result.boxes) {
                nlohmann::json detection;
                
                detection["box"]["left"] = Utils::roundToTwoDecimals(box.left);
                detection["box"]["top"] = Utils::roundToTwoDecimals(box.top);
                detection["box"]["right"] = Utils::roundToTwoDecimals(box.right);
                detection["box"]["bottom"] = Utils::roundToTwoDecimals(box.bottom);
                detection["cls_id"] = box.cls_id;
                detection["score"] = Utils::roundToTwoDecimals(box.score);
                
                if (box.cls_id >= 0 && box.cls_id < static_cast<int>(m_labels.size())) {
                    detection["label"] = m_labels[box.cls_id].first;
                } else {
                    detection["label"] = "unknown";
                }
                
                output["detections"].push_back(detection);
            }
            
        } catch (const std::exception& e) {
            spdlog::error("DetectionAlgorithm inference error: {}", e.what());
            return output;
        }

        return output;
    }

    void visualize(cv::Mat& image, 
                   const algorithms::processors::Result& result) {
        if (image.empty()) {
            spdlog::error("DetectionAlgorithm::visualize - input image is empty");
            return;
        }

        for (const auto& box : result.boxes) {
            try {

                std::string label_text;
                cv::Scalar color;
                
                if (box.cls_id >= 0 && box.cls_id < static_cast<int>(m_labels.size())) {
                    label_text = m_labels[box.cls_id].first;
                } else {
                    label_text = "unknown";
                }
                
                
                int cls_id = std::max(0, box.cls_id); 
                int hue = (cls_id * 137) % 360; 
                
                cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue / 2, 255, 255)); 
                cv::Mat bgr;
                cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
                cv::Vec3b bgr_pixel = bgr.at<cv::Vec3b>(0, 0);
                color = cv::Scalar(bgr_pixel[0], bgr_pixel[1], bgr_pixel[2]);
                
                std::string display_text = label_text + " " + cv::format("%.2f", box.score);
                
                cv::rectangle(image, 
                            cv::Point(static_cast<int>(box.left), static_cast<int>(box.top)),
                            cv::Point(static_cast<int>(box.right), static_cast<int>(box.bottom)),
                            color, 2, cv::LINE_AA);
            
                int baseline = 0;
                cv::Size label_size = cv::getTextSize(display_text, 
                                                     cv::FONT_HERSHEY_SIMPLEX, 
                                                     0.6, 1, &baseline);
                
                int label_top = std::max(static_cast<int>(box.top), label_size.height);
                
                cv::rectangle(image,
                            cv::Point(static_cast<int>(box.left), label_top - label_size.height),
                            cv::Point(static_cast<int>(box.left) + label_size.width, label_top + baseline),
                            color, -1);
                
                cv::putText(image, display_text,
                          cv::Point(static_cast<int>(box.left), label_top),
                          cv::FONT_HERSHEY_SIMPLEX, 0.6,
                          cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
                
            } catch (const std::exception& e) {
                spdlog::error("DetectionAlgorithm::visualize - error processing detection: {}", e.what());
                continue;
            }
        }
        
        spdlog::debug("DetectionAlgorithm::visualize - drew {} detections", result.boxes.size());
    }

    std::string getName() const override {
        return name_;
    }
private:
    std::shared_ptr<NcnnEngine::NcnnInference> detector_;
    std::shared_ptr<algorithms::processors::Preprocessor> preprocessor_;
    std::shared_ptr<algorithms::processors::Postprocessor> postprocessor_;
};

#endif // DETECTION_ALGORITHM_HPP
