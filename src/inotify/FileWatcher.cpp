#include "FileWatcher.hpp"
#include <fstream>


FileWatcher::FileWatcher(const std::string& watch_path, 
                         const std::vector<std::string>& extensions)
    : watch_path_(watch_path),
      is_watching_(false),
      notifier_builder_(nullptr),
      dedup_window_(500) {  
    
    // 转换扩展名为小写并存储
    for (const auto& ext : extensions) {
        std::string lower_ext = ext;
        std::transform(lower_ext.begin(), lower_ext.end(), 
                      lower_ext.begin(), ::tolower);
        extensions_.insert(lower_ext);
    }
    
    if (!extensions_.empty()) {
        std::string ext_list;
        for (const auto& ext : extensions_) {
            ext_list += ext + " ";
        }
    }
}

FileWatcher::~FileWatcher() {
    stop();
}

bool FileWatcher::shouldProcessFile(const std::string& filename) const {
    // 如果没有指定扩展名，处理所有文件
    if (extensions_.empty()) {
        return true;
    }
    
    // 获取文件扩展名
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    
    std::string ext = filename.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return extensions_.find(ext) != extensions_.end();
}

void FileWatcher::handleEvent(const inotify::Notification& notification) {
    // 只处理文件事件，跳过目录
    if (std::filesystem::is_directory(notification.path)) {
        return;
    }
    
    std::string filename = notification.path.filename().string();
    
    // 过滤唤醒信号文件，不触发用户回调
    if (filename == WAKE_FILE_NAME) {
        return;
    }
    
    // 检查文件扩展名
    if (!shouldProcessFile(filename)) {
        return;
    }
    
    FileEvent file_event;
    file_event.file_name = filename;
    file_event.file_path = notification.path.string();
    
    // 根据 inotify 事件类型转换为我们的事件类型
    switch (notification.event) {
        case inotify::Event::create:
            file_event.type = EventType::FILE_CREATED;
            spdlog::debug("File created: {}", filename);
            break;
            
        case inotify::Event::modify:
            file_event.type = EventType::FILE_MODIFIED;
            spdlog::debug("File modified: {}", filename);
            break;
            
        case inotify::Event::close_write:
            file_event.type = EventType::FILE_CLOSED;
            spdlog::debug("File closed (write complete): {}", filename);
            break;
            
        case inotify::Event::remove:
            file_event.type = EventType::FILE_DELETED;
            spdlog::debug("File deleted: {}", filename);
            break;
            
        default:
            // 忽略其他事件
            return;
    }
    
    // 去重检查：防止短时间内同一文件多次触发事件
    {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = last_processed_.find(file_event.file_path);
        
        if (it != last_processed_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
            if (elapsed < dedup_window_) {
                spdlog::debug("Skipping duplicate event for {} ({}ms ago)", file_event.file_name, elapsed.count());
                return; // 跳过重复事件
            }
        }
        
        // 更新最后处理时间
        last_processed_[file_event.file_path] = now;
    }
    
    // 调用回调函数
    if (callback_) {
        try {
            callback_(file_event);
        } catch (const std::exception& e) {
            spdlog::error("Exception in callback: {}", e.what());
        }
    }
}

bool FileWatcher::start(EventCallback callback) {
    if (is_watching_) {
        spdlog::warn("FileWatcher is already running");
        return false;
    }
    
    if (!callback) {
        spdlog::error("Invalid callback function");
        return false;
    }
    
    // 检查监听路径是否存在
    if (!std::filesystem::exists(watch_path_)) {
        spdlog::error("Watch path does not exist: {}", watch_path_);
        return false;
    }
    
    callback_ = callback;
    
    try {
        notifier_builder_ = std::make_unique<inotify::NotifierBuilder>();
        
        auto notifier = notifier_builder_->watchPathRecursively(watch_path_)
            .onEvent(inotify::Event::close_write, [this](const auto& n) { handleEvent(n); })
            .onEvent(inotify::Event::remove, [this](const auto& n) { handleEvent(n); })
            .onUnexpectedEvent([](const auto& n) {
                auto event_val = static_cast<uint32_t>(n.event);
                // 忽略: OPEN(32), ACCESS(1), CLOSE_NOWRITE(16) 及其目录版本
                if (event_val != 1 && event_val != 16 && event_val != 32 && 
                    event_val != 1073741825 && event_val != 1073741840 && event_val != 1073741856) {
                    spdlog::debug("Unhandled inotify event: {}", event_val);
                }
            });
        
        // 监听线程
        is_watching_ = true;
        watch_thread_ = std::thread([notifier, this]() mutable {    
            while (is_watching_) {
                notifier.runOnce();
            }
        });   
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to start FileWatcher: {}", e.what());
        is_watching_ = false;
        notifier_builder_.reset();
        return false;
    }
}

void FileWatcher::stop() {
    if (!is_watching_) {
        return;
    }
    
    spdlog::info("Stopping FileWatcher for path: {}", watch_path_);
    is_watching_ = false;
    
    wakeUpWatchThread();
    
    if (watch_thread_.joinable()) {
        auto start_time = std::chrono::steady_clock::now();
        bool thread_stopped = false;
        
        while (watch_thread_.joinable()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= STOP_TIMEOUT) {
                spdlog::warn("FileWatcher thread did not stop within {}s, detaching...", 
                           STOP_TIMEOUT.count());
                watch_thread_.detach();
                break;
            }
            
            std::this_thread::yield();
        }
        
        if (watch_thread_.joinable()) {
            watch_thread_.join();
            thread_stopped = true;
        }
        
        if (thread_stopped) {
            spdlog::debug("FileWatcher thread stopped gracefully");
        }
    }
    
    // 清理资源
    notifier_builder_.reset();
    
    spdlog::info("FileWatcher stopped successfully");
}

void FileWatcher::wakeUpWatchThread() {
    try {
        std::filesystem::path wake_file_path = 
            std::filesystem::path(watch_path_) / WAKE_FILE_NAME;
        
        {
            std::ofstream wake_file(wake_file_path);
            wake_file << "wake";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (std::filesystem::exists(wake_file_path)) {
            std::filesystem::remove(wake_file_path);
        }
        
        spdlog::debug("Sent wake-up signal to FileWatcher thread");
        
    } catch (const std::exception& e) {
        spdlog::warn("Failed to wake up FileWatcher thread: {}", e.what());
    }
}