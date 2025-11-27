//
// Created for ncnn_demo - File Watcher based on inotify-cpp
//

#ifndef FILE_WATCHER_HPP
#define FILE_WATCHER_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>
#include <inotify-cpp/NotifierBuilder.h>
#include <inotify-cpp/FileSystemAdapter.h>

class FileWatcher {
public:
    /**
     * @brief 文件事件类型
     */
    enum class EventType {
        FILE_CREATED,   // 文件创建
        FILE_MODIFIED,  // 文件修改
        FILE_DELETED,   // 文件删除
        FILE_CLOSED     // 文件关闭（写入完成）
    };

    struct FileEvent {
        EventType type;
        std::string file_path;  
        std::string file_name;  
    };
    
    /**
     * @brief 事件回调函数类型
     */
    using EventCallback = std::function<void(const FileEvent&)>;
    
    /**
     * @brief 构造函数
     * @param extensions 要监听的文件扩展名（如 {".jpg", ".png"}），为空则监听所有文件
     */
    explicit FileWatcher(const std::string& watch_path, 
                        const std::vector<std::string>& extensions = {});
    
    ~FileWatcher();
    
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    bool start(EventCallback callback);
    
    void stop();
    
    bool isWatching() const { return is_watching_; }
    
    std::string getWatchPath() const { return watch_path_; }
    
private:
    bool shouldProcessFile(const std::string& filename) const;
    
    /**
     * @brief 处理 inotify 事件
     */
    void handleEvent(const inotify::Notification& notification);
    
    void wakeUpWatchThread();
    
private:
    std::string watch_path_;                    
    std::unordered_set<std::string> extensions_; // 监听的文件扩展名
    
    std::atomic<bool> is_watching_;            
    std::thread watch_thread_;                 
    
    EventCallback callback_;                    
    
    std::unique_ptr<inotify::NotifierBuilder> notifier_builder_; // inotify-cpp 构建器
    
    // 去重机制
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_processed_; 
    std::mutex dedup_mutex_;                   
    std::chrono::milliseconds dedup_window_;
    
    // 优雅退出机制
    static constexpr auto STOP_TIMEOUT = std::chrono::seconds(3); 
    static constexpr const char* WAKE_FILE_NAME = ".filewatcher_wake_signal";   
};

#endif // FILE_WATCHER_HPP