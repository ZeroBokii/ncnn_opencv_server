#include "NcnnInference.hpp"
#include <opencv2/imgproc.hpp>

using namespace NcnnEngine;

NcnnInference::NcnnInference(const std::string &model_param, const std::string &model_bin,
                 const std::string &in_name, const std::string &out_name) 
    : conf_threshold_(0.25f),
    nms_threshold_(0.45f),
    is_initialized_(false),
    input_layer_name_(in_name),
    output_layer_name_(out_name),
    model_param_path_(model_param),
    model_bin_path_(model_bin) {
    
    #if NCNN_VULKAN
    vulkan_available_ = ncnn::get_gpu_count() > 0;
    #else
    vulkan_available_ = 0;
    #endif
    
}

NcnnInference::~NcnnInference() {
    if (is_initialized_) {
        NCNN_NET_.clear();
    }
}

bool NcnnInference::initialize() {
    try {
        NCNN_NET_.opt.num_threads = 4;
        NCNN_NET_.opt.use_fp16_storage = true;
        NCNN_NET_.opt.use_packing_layout = true;
        NCNN_NET_.opt.use_local_pool_allocator = false;  
        
        if (vulkan_available_) {
            NCNN_NET_.opt.use_vulkan_compute = true;
        } else {
            NCNN_NET_.opt.use_vulkan_compute = false;
        }
        
        if (use_int8_) {
            NCNN_NET_.opt.use_int8_inference = true;
        } else {
            NCNN_NET_.opt.use_int8_inference = false;
        }
        
        if (!loadModel(model_param_path_, model_bin_path_)) {
            spdlog::error("Failed to load model");
            return false;
        } 
        
        is_initialized_ = true;
        
        spdlog::info("========================================");
        spdlog::info("✓ NcnnInference Initialized Successfully");
        spdlog::info("  Model param: {}", model_param_path_);
        spdlog::info("  Model bin: {}", model_bin_path_);
        spdlog::info("  Input Layer: {}", input_layer_name_);
        spdlog::info("  Output Layer: {}", output_layer_name_);
        spdlog::info("  Vulkan: {}", vulkan_available_);
        spdlog::info("  INT8: {}", use_int8_);
        spdlog::info("========================================");

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Initialization failed: {}", e.what());
        return false;
    }
}


bool NcnnInference::loadModel(const std::string& param_path, const std::string& bin_path) {
    if (param_path.empty() || bin_path.empty()) {
        spdlog::error("Model paths are empty");
        return false;
    }
    
    int ret = NCNN_NET_.load_param(param_path.c_str());
    if (ret != 0) {
        spdlog::error("Failed to load param file: {} (error code: {})", param_path, ret);
        return false;
    }
    
    ret = NCNN_NET_.load_model(bin_path.c_str());
    if (ret != 0) {
        spdlog::error("Failed to load model file: {} (error code: {})", bin_path, ret);
        return false;
    }
    
    return true;
}

ncnn::Mat NcnnInference::infer(const ncnn::Mat& input_tensor) {
    ncnn::Mat output;

    if (!is_initialized_) {
        spdlog::error("NcnnInference not initialized, cannot perform inference");
        return output;
    }
    
    if (input_tensor.empty()) { return output; }
    try {
        ncnn::Extractor ex = NCNN_NET_.create_extractor();
        int ret = ex.input(input_layer_name_.c_str(), input_tensor);
        if (ret != 0) {
            spdlog::error("Failed to set input '{}' (error code: {})", input_layer_name_, ret);
            return output;
        }
        ret = ex.extract(output_layer_name_.c_str(), output);
        if (ret != 0) {
            spdlog::error("Failed to extract output '{}' (error code: {})", output_layer_name_, ret);
            return output;
        }
        return output;
    } catch (const std::exception& e) {
        spdlog::error("Inference failed with exception: {}", e.what());
        return output;
    }
}