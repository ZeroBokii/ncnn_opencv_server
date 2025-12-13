#include "HttpApiServer.hpp"
#include <chrono>

HttpApiServer::HttpApiServer(const std::string& host, int port)
    : host_(host)
    , port_(port)
    , server_(std::make_unique<httplib::Server>())
    , is_running_(false) {
    
    setupRoutes();
    spdlog::debug("[HttpApiServer] Created, will listen on {}:{}", host_, port_);
}

HttpApiServer::~HttpApiServer() {
    stop();
}

void HttpApiServer::setupRoutes() {
    server_->Post("/api/inference/toggle", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("enabled") || !body["enabled"].is_boolean()) {
                res.status = 400;
                res.set_content(createJsonResponse(false, 
                    "Missing or invalid 'enabled' field (boolean required)"), "application/json");
                return;
            }
            
            bool enabled = body["enabled"].get<bool>();
            auto& switch_instance = InferenceSwitch::getInstance();
            switch_instance.setEnabled(enabled);
            
            nlohmann::json data;
            data["inference_enabled"] = switch_instance.isEnabled();
            
            std::string message = enabled ? "Inference enabled" : "Inference disabled";
            res.set_content(createJsonResponse(true, message, data), "application/json");
            spdlog::info("[HttpApiServer] POST /api/inference/toggle -> {}", 
                        enabled ? "enabled" : "disabled");
            
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            res.set_content(createJsonResponse(false, 
                std::string("Invalid JSON: ") + e.what()), "application/json");
        }
    });
}

std::string HttpApiServer::createJsonResponse(bool success, const std::string& message, 
                                              const nlohmann::json& data) {
    nlohmann::json response;
    response["success"] = success;
    response["message"] = message;
    
    if (!data.is_null()) {
        response["data"] = data;
    }
    
    return response.dump();
}

bool HttpApiServer::start() {
    if (is_running_.load()) {
        spdlog::warn("[HttpApiServer] Server is already running");
        return false;
    }
    
    server_thread_ = std::thread(&HttpApiServer::serverThreadFunc, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (is_running_.load()) {
        spdlog::info("[HttpApiServer] Started on http://{}:{}", host_, port_);
        return true;
    }
    
    spdlog::error("[HttpApiServer] Failed to start server");
    return false;
}

void HttpApiServer::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    spdlog::info("[HttpApiServer] Stopping server...");
    
    server_->stop();
    is_running_.store(false);
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    spdlog::info("[HttpApiServer] Server stopped");
}

void HttpApiServer::serverThreadFunc() {
    is_running_.store(true);
    
    bool result = server_->listen(host_.c_str(), port_);
    
    if (!result) {
        spdlog::error("[HttpApiServer] Failed to listen on {}:{}", host_, port_);
    }
    
    is_running_.store(false);
}
