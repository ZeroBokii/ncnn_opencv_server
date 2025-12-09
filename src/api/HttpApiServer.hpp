#ifndef HTTP_API_SERVER_HPP
#define HTTP_API_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// 禁用 httplib 的 OpenSSL 支持（避免链接 SSL 库）
#define CPPHTTPLIB_NO_OPENSSL
#include <httplib.h>

#include "InferenceSwitch.hpp"

/**
 * @brief HTTP API 服务器 - 提供推理控制的 REST API
 * 
 * API 端点：
 * - POST /api/inference/toggle   - 切换推理状态
 */
class HttpApiServer {
public:
    /**
     * @brief 构造函数
     * @param host 监听地址，默认 "0.0.0.0"
     * @param port 监听端口，默认 8080
     */
    explicit HttpApiServer(const std::string& host = "0.0.0.0", int port = 8080);
    
    ~HttpApiServer();
    
    // 禁用拷贝
    HttpApiServer(const HttpApiServer&) = delete;
    HttpApiServer& operator=(const HttpApiServer&) = delete;
    
    /**
     * @brief 启动 HTTP 服务器（非阻塞，在独立线程中运行）
     * @return true 如果启动成功
     */
    bool start();
    
    /**
     * @brief 停止 HTTP 服务器
     */
    void stop();
    
    /**
     * @brief 检查服务器是否正在运行
     */
    bool isRunning() const { return is_running_.load(); }
    
    /**
     * @brief 获取监听端口
     */
    int getPort() const { return port_; }
    
    /**
     * @brief 获取监听地址
     */
    std::string getHost() const { return host_; }

private:
    /**
     * @brief 注册所有 API 路由
     */
    void setupRoutes();
    
    /**
     * @brief 创建 JSON 响应
     */
    static std::string createJsonResponse(bool success, const std::string& message, 
                                          const nlohmann::json& data = nullptr);
    
    /**
     * @brief 服务器线程函数
     */
    void serverThreadFunc();

private:
    std::string host_;
    int port_;
    
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> is_running_;
};

#endif // HTTP_API_SERVER_HPP
