#ifndef LAZY_FILE_SINK_HPP
#define LAZY_FILE_SINK_HPP

#include <string>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

template<typename Mutex>
class lazy_file_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit lazy_file_sink(const std::string& filename)
        : filename_(filename), file_created_(false) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (msg.level < spdlog::level::err) {
            return;
        }
        
        if (!file_created_) {
            try {
                real_sink_ = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename_, true);
                real_sink_->set_level(spdlog::level::err);
                // 复制 formatter（如果存在）
                if (this->formatter_) {
                    real_sink_->set_formatter(this->formatter_->clone());
                }
                file_created_ = true;
            } catch (const std::exception& e) {
                return;
            }
        }
        
        if (real_sink_) {
            real_sink_->log(msg);
        }
    }

    void flush_() override {
        if (real_sink_) {
            real_sink_->flush();
        }
    }

private:
    std::string filename_;
    bool file_created_;
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> real_sink_;
};

#endif // LAZY_FILE_SINK_HPP