#ifndef PROCESSOR_BASE_HPP
#define PROCESSOR_BASE_HPP

#include <opencv2/opencv.hpp>
#include <ncnn/net.h>
#include <nlohmann/json.hpp>
#include <string>

namespace algorithms {
namespace processors {

struct PreprocessResult {
    ncnn::Mat preprocessed_mat;  
    float scale;                
    int wpad;                    
    int hpad;                    
    int original_width;        
    int original_height;     
    
    PreprocessResult()
        : scale(1.0f), wpad(0), hpad(0), original_width(0), original_height(0) {}
};

class Preprocessor {
public:
    virtual ~Preprocessor() = default;
    
    virtual PreprocessResult process(const cv::Mat& image) = 0;
    virtual std::string getName() const = 0;
};

struct Box {
    float left;
    float top;
    float right;
    float bottom;
    int cls_id;
    float score;
    Box() : left(0), top(0), right(0), bottom(0), cls_id(-1), score(0) {}
};

/**
 * @brief 后处理结果 
 */
struct Result {
    std::vector<Box> boxes;
    Result() = default;
};

class Postprocessor {
public:
    virtual ~Postprocessor() = default;
    
    virtual Result process(const ncnn::Mat& output, 
                                   const PreprocessResult& preprocess_result) = 0;
    virtual std::string getName() const = 0;
};

} // namespace processors
} // namespace algorithms

#endif // PROCESSOR_BASE_HPP