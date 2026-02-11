#pragma once
// Modern C++ Structured Logging Library (cLog core)
// Async by default, chainable API, zero macros, extensible sinks, structured JSON output
// Author: cLog contributors    License: MIT

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <mutex>
#include <thread>
#include "spsc_ring_buffer.hpp"
#include <condition_variable>
#include <cstdio>
#include <utility>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace c_log {

// Log levels (compile-time support possible)
enum class Level : int {
    Trace = 0, Debug = 1, Info = 2, Warning = 3, Error = 4, Critical = 5, None = 6
};

struct Sink {
    virtual ~Sink() = default;
    virtual void log(const std::string &message) = 0;
};

class ConsoleSink : public Sink {
public:
    void log(const std::string& msg) override {
        std::fprintf(stderr, "%s\n", msg.c_str());
    }
};

class Logger {
public:
    enum class Mode { Async, Sync };
    explicit Logger(Mode mode = Mode::Async)
        : mode_(mode), min_level_(Level::Info), stop_(false), worker_started_(false) {
        sinks_.emplace_back(std::make_unique<ConsoleSink>()); // default sink
    }
    ~Logger() {
        flush_if_building();
        stop_ = true;
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger& set_level(Level lvl) { min_level_ = lvl; return *this; }
    Logger& add_sink(std::unique_ptr<Sink> sink) {
        sinks_.emplace_back(std::move(sink));
        return *this;
    }
    // Chainable log entry; event type and value
    Logger& info(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Info};
        return *this;
    }
    Logger& debug(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Debug};
        return *this;
    }
    Logger& warning(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Warning};
        return *this;
    }
    Logger& warn(const std::string& event) { return warning(event); }
    Logger& error(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Error};
        return *this;
    }
    Logger& trace(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Trace};
        return *this;
    }
    Logger& critical(const std::string& event) {
        flush_if_building();
        cur_entry_ = Entry{event, Level::Critical};
        return *this;
    }
    Logger& kv(const std::string& key, const std::string& val) {
        cur_entry_.fields.emplace_back(key, val);
        return *this;
    }
    Logger& kv(const std::string& key, int val) {
        cur_entry_.fields.emplace_back(key, std::to_string(val));
        return *this;
    }

private:
    struct Entry {
        std::string event;
        Level level;
        std::vector<std::pair<std::string, std::string>> fields;
    };
    std::vector<std::unique_ptr<Sink>> sinks_;
    Level min_level_;
    Mode mode_;
    // Async machinery
    SPSCRingBuffer<Entry> queue_{1024}; // Pre-allocate space for 1024 entries
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic<bool> stop_;
    bool worker_started_;
    Entry cur_entry_{};

    static void escape_json_string(const std::string& input, std::string& output) {
        for (auto c : input) {
            switch (c) {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        output += buf;
                    } else {
                        output += c;
                    }
            }
        }
    }

    void emit_entry(const Entry& entry) {
        std::string line;
        // Pre-allocate to reduce reallocations. 
        // Heuristic: event + overhead + fields * (key+val+overhead)
        line.reserve(64 + entry.event.size() + entry.fields.size() * 32); 

        line += "{\"event\":\"";
        escape_json_string(entry.event, line);
        line += "\"";

        for (const auto& kv : entry.fields) {
            line += ",\"";
            escape_json_string(kv.first, line);
            line += "\":\"";
            escape_json_string(kv.second, line);
            line += "\"";
        }
        line += "}";

        for (const auto& s : sinks_) s->log(line);
    }
    void flush_if_building() {
        if (!cur_entry_.event.empty()) {
            if (mode_ == Mode::Async) {
                start_worker_if_needed();
                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    queue_.push(cur_entry_);
                }
                cv_.notify_one();
            } else {
                emit_entry(cur_entry_);
            }
        }
        cur_entry_ = Entry{};
    }
    void start_worker_if_needed() {
        if (worker_started_) return;
        worker_started_ = true;
        worker_ = std::thread([this] { run_async(); });
    }
    void run_async() {
        while (true) {
            Entry entry;
            std::optional<Entry> result;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait(lk, [&] {
                    result = queue_.pop();
                    return stop_ || result.has_value();
                });
                if (!result.has_value()) {
                    if (stop_) {
                        break;
                    } else {
                        continue;
                    }
                }
                entry = std::move(result.value());
            }
            emit_entry(entry);
            // After emitting, if stop_ and queue is empty, exit
            std::unique_lock<std::mutex> lk(mutex_);
            if (stop_ && !queue_.pop().has_value()) {
                break;
            }
        }
}
};

} // namespace clog
