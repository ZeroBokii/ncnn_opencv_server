#ifndef INFERENCE_SWITCH_HPP
#define INFERENCE_SWITCH_HPP

#include <atomic>
#include <spdlog/spdlog.h>

/**
 * @brief 推理开关管理器 - 线程安全的全局推理状态控制
 */
class InferenceSwitch {
public:
    static InferenceSwitch& getInstance() {
        static InferenceSwitch instance;
        return instance;
    }
    
    InferenceSwitch(const InferenceSwitch&) = delete;
    InferenceSwitch& operator=(const InferenceSwitch&) = delete;
    
    bool isEnabled() const {
        return enabled_.load(std::memory_order_acquire);
    }
    
    void setEnabled(bool enabled) {
        bool was_enabled = enabled_.exchange(enabled, std::memory_order_acq_rel);
        if (was_enabled != enabled) {
            spdlog::info("[InferenceSwitch] Inference {}", enabled ? "ENABLED" : "DISABLED");
        }
    }

private:
    InferenceSwitch() : enabled_(true) {}
    
    std::atomic<bool> enabled_;
};

#endif // INFERENCE_SWITCH_HPP
