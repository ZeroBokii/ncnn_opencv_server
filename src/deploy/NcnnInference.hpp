
#pragma once

#include <string>

#include <ncnn/net.h>
#include <opencv2/core/core.hpp>
#include <spdlog/spdlog.h>

namespace NcnnEngine {

struct NcnnConfig {
    std::string param_path;
    std::string bin_path;
    bool use_int8 = false;
    std::string input_layer_name = "in0";
    std::string output_layer_name = "out0";
};

struct NcnnBox {
    int left;
    int top;
    int right;
    int bottom;
    float score;
    int class_id;
    float masks;
    NcnnBox()
        : left(0), top(0), right(0), bottom(0), score(0.0f), class_id(0), masks(0.0f)
    {}
};

struct NcnnResult {
    std::vector<NcnnBox> boxes;
};

class NcnnInference {
public:
    NcnnInference(const std::string &model_param, const std::string &model_bin,
            const std::string &in_name, const std::string &out_name);
    ~NcnnInference();

    NcnnInference(const NcnnInference&) = delete;
    NcnnInference& operator=(const NcnnInference&) = delete;

    bool initialize();

    bool isInitialized() const { return is_initialized_; }

    void setUseInt8(bool use_int8) { use_int8_ = use_int8; }

    ncnn::Mat infer(const ncnn::Mat& input_tensor);

private:
    ncnn::Net NCNN_NET_;
    int vulkan_available_;
    
    bool use_int8_ = false;

    float conf_threshold_;
    float nms_threshold_;
    bool is_initialized_;
    
    std::string model_param_path_;
    std::string model_bin_path_;
    
    std::string input_layer_name_;
    std::string output_layer_name_;

private:
    bool loadModel(const std::string& param_path, const std::string& bin_path);
};

} // namespace NcnnEngine