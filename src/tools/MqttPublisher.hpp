#ifndef MQTT_PUBLISHER_HPP
#define MQTT_PUBLISHER_HPP

#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>
#include <mosquitto.h>

/**
 * @brief MQTT 发布器 - 发送缺陷检测通知
 */
class MqttPublisher {
public:
    struct Config {
        std::string broker_host = "127.0.0.1";
        int broker_port = 1883;
        std::string client_id = "ncnn_inference_server";
        std::string topic = "opi/zero2/events/target_detected";
        int qos = 1;
        int keep_alive = 30;
    };

    explicit MqttPublisher(const Config& config) : config_(config) {
        mosquitto_lib_init();
        mosq_ = mosquitto_new(config_.client_id.c_str(), true, this);
        if (mosq_) {
            mosquitto_connect_callback_set(mosq_, onConnect);
            mosquitto_disconnect_callback_set(mosq_, onDisconnect);
            mosquitto_reconnect_delay_set(mosq_, 1, 10, true);
        }
    }

    ~MqttPublisher() {
        disconnect();
        if (mosq_) mosquitto_destroy(mosq_);
        mosquitto_lib_cleanup();
    }

    MqttPublisher(const MqttPublisher&) = delete;
    MqttPublisher& operator=(const MqttPublisher&) = delete;

    bool connect() {
        if (!mosq_ || connected_.load()) return connected_.load();
        
        int rc = mosquitto_connect(mosq_, config_.broker_host.c_str(), config_.broker_port, config_.keep_alive);
        if (rc != MOSQ_ERR_SUCCESS) {
            spdlog::error("[MQTT] Connect failed: {}", mosquitto_strerror(rc));
            return false;
        }
        
        mosquitto_loop_start(mosq_);
        
        for (int i = 0; i < 30 && !connected_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return connected_.load();
    }

    void disconnect() {
        if (!mosq_) return;
        if (connected_.load()) mosquitto_disconnect(mosq_);
        mosquitto_loop_stop(mosq_, true);
        connected_.store(false);
    }

    bool isConnected() const { return connected_.load(); }

    /**
     * @brief 发送"检测到缺陷"消息
     */
    bool publishDefectDetected() {
        if (!mosq_) return false;
        
        const char* msg = "检测到缺陷";
        int rc = mosquitto_publish(mosq_, nullptr, config_.topic.c_str(),
                                   static_cast<int>(std::strlen(msg)), msg,
                                   config_.qos, false);
        
        if (rc != MOSQ_ERR_SUCCESS) {
            spdlog::error("[MQTT] Publish failed: {}", mosquitto_strerror(rc));
            return false;
        }
        
        spdlog::debug("[MQTT] 已发送: {}", msg);
        return true;
    }

private:
    static void onConnect(struct mosquitto*, void* userdata, int rc) {
        auto* self = static_cast<MqttPublisher*>(userdata);
        if (rc == MOSQ_ERR_SUCCESS) {
            self->connected_.store(true);
            spdlog::info("[MQTT] Connected");
        } else {
            spdlog::error("[MQTT] Connect failed: {}", mosquitto_strerror(rc));
        }
    }

    static void onDisconnect(struct mosquitto*, void* userdata, int rc) {
        auto* self = static_cast<MqttPublisher*>(userdata);
        self->connected_.store(false);
        if (rc != MOSQ_ERR_SUCCESS) {
            spdlog::warn("[MQTT] Connection lost");
        }
    }

    Config config_;
    struct mosquitto* mosq_ = nullptr;
    std::atomic<bool> connected_{false};
};

#endif // MQTT_PUBLISHER_HPP
