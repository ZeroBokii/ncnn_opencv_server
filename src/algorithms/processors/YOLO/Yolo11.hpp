#ifndef YOLO11_HPP
#define YOLO11_HPP

#include "../ProcessorBase.hpp"
#include <ncnn/layer.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <float.h>
#include <spdlog/spdlog.h>

namespace algorithms {
namespace processors {

/**
 * @brief YOLO11 后处理器 - 使用 DFL (Distribution Focal Loss) 边界框解码
 * 
 * YOLO11 输出格式：
 * - 形状：[h, w] 其中 h=8400 (锚点总数), w=144
 * - 前 64 维：DFL 分布 (reg_max=16, 4个边 -> 16*4=64)
 * - 后 80 维：类别分数（无 objectness，直接 sigmoid）
 * 
 * 多尺度锚点分布：
 * - Stride 8:  80x80 = 6400 个锚点
 * - Stride 16: 40x40 = 1600 个锚点  
 * - Stride 32: 20x20 = 400 个锚点
 * 总计：8400 个锚点
 */
class Yolo11Postprocessor : public Postprocessor {
public:
    explicit Yolo11Postprocessor(float conf_threshold = 0.25f, float nms_threshold = 0.45f)
        : conf_threshold_(conf_threshold), nms_threshold_(nms_threshold) {
        // YOLO11 DFL 参数
        reg_max_ = 16;
        
        // 多尺度步长
        strides_ = {8, 16, 32};
        
        spdlog::debug("Yolo11Postprocessor created: conf={}, nms={}, reg_max={}",
                     conf_threshold_, nms_threshold_, reg_max_);
    }
    
    Result process(const ncnn::Mat& output, 
                   const PreprocessResult& preprocess_result) override {
        Result result;
        
        if (output.empty()) {
            spdlog::warn("Yolo11Postprocessor: empty output tensor");
            return result;
        }
        
        // YOLO11 输出：[8400, 144]
        // h = 锚点数量, w = 特征维度
        int num_proposals = output.h;  // 8400
        int feat_dim = output.w;        // 144
        
        int num_classes = feat_dim - reg_max_ * 4;  // 144 - 64 = 80
        
        spdlog::debug("Yolo11Postprocessor: output shape=[{}, {}], num_classes={}",
                     num_proposals, feat_dim, num_classes);
        
        if (num_classes <= 0) {
            spdlog::error("Yolo11Postprocessor: Invalid num_classes={}", num_classes);
            return result;
        }
        
        std::vector<Detection> detections;
        
        // 根据不同 stride 处理锚点
        int proposal_offset = 0;
        const int pad_w = preprocess_result.preprocessed_mat.w;
        const int pad_h = preprocess_result.preprocessed_mat.h;
        
        for (int stride : strides_) {
            int num_grid_x = pad_w / stride;
            int num_grid_y = pad_h / stride;
            int num_grid = num_grid_x * num_grid_y;
            
            generate_proposals(output, proposal_offset, num_grid, 
                             num_grid_x, num_grid_y, stride,
                             num_classes, detections);
            
            proposal_offset += num_grid;
        }
        
        spdlog::debug("Yolo11Postprocessor: {} detections before NMS", detections.size());
        
        std::vector<Detection> final_detections = applyNMS(detections);
        
        spdlog::debug("Yolo11Postprocessor: {} detections after NMS", final_detections.size());
        
        // 转换为输出格式并还原到原始坐标
        result.boxes.reserve(final_detections.size());
        for (const auto& det : final_detections) {
            float x_orig = (det.x - preprocess_result.wpad / 2.0f) / preprocess_result.scale;
            float y_orig = (det.y - preprocess_result.hpad / 2.0f) / preprocess_result.scale;
            float w_orig = det.width / preprocess_result.scale;
            float h_orig = det.height / preprocess_result.scale;
            
            // 裁剪到图像边界
            x_orig = std::max(0.0f, std::min(x_orig, static_cast<float>(preprocess_result.original_width - 1)));
            y_orig = std::max(0.0f, std::min(y_orig, static_cast<float>(preprocess_result.original_height - 1)));
            
            float x1 = x_orig;
            float y1 = y_orig;
            float x2 = std::min(x_orig + w_orig, static_cast<float>(preprocess_result.original_width - 1));
            float y2 = std::min(y_orig + h_orig, static_cast<float>(preprocess_result.original_height - 1));
            
            Box box;
            box.left = x1;
            box.top = y1;
            box.right = x2;
            box.bottom = y2;
            box.cls_id = det.class_id;
            box.score = det.confidence;
            
            result.boxes.push_back(box);
        }
        
        return result;
    }
    
    std::string getName() const override {
        return "yolo11";
    }
    
private:
    struct Detection {
        float x, y, width, height; 
        float confidence;
        int class_id;
    };
    
    /**
     * @brief Sigmoid 激活函数
     */
    static inline float sigmoid(float x) {
        return 1.0f / (1.0f + expf(-x));
    }
    
    /**
     * @brief 对单个 stride 层级生成候选框
     */
    void generate_proposals(const ncnn::Mat& pred, 
                           int offset,
                           int num_grid,
                           int num_grid_x,
                           int num_grid_y,
                           int stride,
                           int num_classes,
                           std::vector<Detection>& detections) const {
        
        for (int y = 0; y < num_grid_y; y++) {
            for (int x = 0; x < num_grid_x; x++) {
                int anchor_idx = offset + y * num_grid_x + x;
                
                if (anchor_idx >= pred.h) {
                    continue;
                }
                
                const float* pred_data = pred.row(anchor_idx);
                
                // 1. 解析类别分数 (后80维，需要 sigmoid)
                int label = -1;
                float max_score = -FLT_MAX;
                
                for (int c = 0; c < num_classes; c++) {
                    float score = pred_data[reg_max_ * 4 + c];
                    if (score > max_score) {
                        max_score = score;
                        label = c;
                    }
                }
                
                float confidence = sigmoid(max_score);
                
                if (confidence < conf_threshold_) {
                    continue;
                }
                
                float pred_ltrb[4] = {0};
                
                for (int k = 0; k < 4; k++) {
                    float dist = decode_dfl(pred_data + k * reg_max_, reg_max_);
                    pred_ltrb[k] = dist * stride;
                }
                
                float center_x = (x + 0.5f) * stride;
                float center_y = (y + 0.5f) * stride;
                
                float x0 = center_x - pred_ltrb[0];  // left
                float y0 = center_y - pred_ltrb[1];  // top
                float x1 = center_x + pred_ltrb[2];  // right
                float y1 = center_y + pred_ltrb[3];  // bottom
                
                Detection det;
                det.x = x0;
                det.y = y0;
                det.width = x1 - x0;
                det.height = y1 - y0;
                det.confidence = confidence;
                det.class_id = label;
                
                detections.push_back(det);
            }
        }
    }
    
    /**
     * @brief DFL (Distribution Focal Loss) 解码
     * 对16个分布值做 softmax，然后加权求和得到距离
     */
    float decode_dfl(const float* dfl_data, int reg_max) const {
        // 使用 NCNN Softmax 层
        ncnn::Mat dfl_mat(reg_max);
        memcpy(dfl_mat.data, dfl_data, reg_max * sizeof(float));
        
        // Softmax
        ncnn::Layer* softmax = ncnn::create_layer("Softmax");
        ncnn::ParamDict pd;
        pd.set(0, 0);  // axis = 0
        pd.set(1, 1);  // fix_bug_0159 = 1
        softmax->load_param(pd);
        
        ncnn::Option opt;
        opt.num_threads = 1;
        opt.use_packing_layout = false;
        
        softmax->create_pipeline(opt);
        softmax->forward_inplace(dfl_mat, opt);
        softmax->destroy_pipeline(opt);
        delete softmax;
        
        // 加权求和：sum(i * prob[i])
        float distance = 0.0f;
        const float* prob = (const float*)dfl_mat.data;
        for (int i = 0; i < reg_max; i++) {
            distance += i * prob[i];
        }
        
        return distance;
    }
    
    /**
     * @brief 计算 IoU
     */
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
    
    /**
     * @brief NMS (Non-Maximum Suppression)
     */
    std::vector<Detection> applyNMS(std::vector<Detection>& detections) const {
        if (detections.empty()) {
            return detections;
        }
        
        // 按置信度降序排序
        std::sort(detections.begin(), detections.end(),
                 [](const Detection& a, const Detection& b) {
                     return a.confidence > b.confidence;
                 });
        
        std::vector<bool> suppressed(detections.size(), false);
        std::vector<Detection> result;
        
        for (size_t i = 0; i < detections.size(); i++) {
            if (suppressed[i]) continue;
            
            result.push_back(detections[i]);
            
            // 抑制同类别的重叠框
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
    int reg_max_;  // DFL 参数，YOLO11 使用 16
    std::vector<int> strides_;  // 多尺度步长
};

} // namespace processors
} // namespace algorithms

#endif // YOLO11_HPP