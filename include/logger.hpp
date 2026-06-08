#pragma once
// Modern C++ Structured Logging Library (cLog++)
// Async by default, chainable API, zero macros, extensible sinks, structured JSON output.
// Author: cLog++ contributors    License: MIT

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <chrono>
#include <utility>
#include "spsc_ring_buffer.hpp"

namespace c_log {

// Log levels, ordered by severity. None disables all output.
enum class Level : int {
    Trace = 0, Debug = 1, Info = 2, Warning = 3, Error = 4, Critical = 5, None = 6
};

inline const char* level_name(Level lvl) {
    switch (lvl) {
        case Level::Trace:    return "trace";
        case Level::Debug:    return "debug";
        case Level::Info:     return "info";
        case Level::Warning:  return "warning";
        case Level::Error:    return "error";
        case Level::Critical: return "critical";
        default:              return "none";
    }
}

// A sink receives one fully-serialized JSON line per log entry.
struct Sink {
    virtual ~Sink() = default;
    virtual void log(const std::string& message) = 0;
    // Optional: flush any buffered output durably. Default is a no-op so
    // existing custom sinks keep compiling unchanged.
    virtual void flush() {}
};

// Default sink: writes to stderr (keeps stdout free for program output).
class ConsoleSink : public Sink {
public:
    explicit ConsoleSink(std::FILE* stream = stderr) : stream_(stream) {}
    void log(const std::string& msg) override { std::fprintf(stream_, "%s\n", msg.c_str()); }
    void flush() override { std::fflush(stream_); }
private:
    std::FILE* stream_;
};

class Logger;

// A single log entry, built incrementally then emitted when it goes out of
// scope. Each statement gets its own record (returned by value from the level
// methods), so two threads logging on the same Logger never share record state.
class LogRecord {
public:
    LogRecord(Logger* logger, Level level, std::string event, bool active);
    ~LogRecord();

    // Movable (so the level methods can return it by value), non-copyable.
    LogRecord(LogRecord&& other) noexcept;
    LogRecord& operator=(LogRecord&&) = delete;
    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;

    // Add a structured field. Strings are JSON-escaped and quoted; numbers and
    // bools are emitted as native JSON values (e.g. {"n":42}, not {"n":"42"}).
    LogRecord& kv(const std::string& key, const std::string& val) {
        return add_quoted(key, val);
    }
    LogRecord& kv(const std::string& key, const char* val) {
        return add_quoted(key, val ? std::string(val) : std::string());
    }
    LogRecord& kv(const std::string& key, bool val) {
        return add_raw(key, val ? "true" : "false");
    }
    LogRecord& kv(const std::string& key, int val)                { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, long val)               { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, long long val)          { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, unsigned val)           { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, unsigned long val)      { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, unsigned long long val) { return add_raw(key, std::to_string(val)); }
    LogRecord& kv(const std::string& key, double val) {
        return add_raw(key, encode_double(val));
    }

private:
    friend class Logger;

    struct Entry {
        std::string event;
        Level level = Level::Info;
        std::chrono::system_clock::time_point time;
        // key -> already-encoded JSON value (quoted+escaped for strings,
        // bare for numbers/bools).
        std::vector<std::pair<std::string, std::string>> fields;
    };

    LogRecord& add_quoted(const std::string& key, const std::string& val) {
        if (active_) {
            std::string enc;
            enc.reserve(val.size() + 2);
            enc += '"';
            escape_json(val, enc);
            enc += '"';
            entry_.fields.emplace_back(key, std::move(enc));
        }
        return *this;
    }
    LogRecord& add_raw(const std::string& key, std::string encoded) {
        if (active_) entry_.fields.emplace_back(key, std::move(encoded));
        return *this;
    }

    static std::string encode_double(double v) {
        if (!std::isfinite(v)) return "null"; // JSON has no NaN/Infinity
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.17g", v);
        return std::string(buf);
    }

    static void escape_json(const std::string& input, std::string& output) {
        for (char c : input) {
            switch (c) {
                case '"':  output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b";  break;
                case '\f': output += "\\f";  break;
                case '\n': output += "\\n";  break;
                case '\r': output += "\\r";  break;
                case '\t': output += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned>(static_cast<unsigned char>(c)));
                        output += buf;
                    } else {
                        output += c;
                    }
            }
        }
    }

    Logger* logger_;
    Entry entry_;
    bool active_;
};

class Logger {
public:
    enum class Mode { Async, Sync };

    explicit Logger(Mode mode = Mode::Async, std::size_t capacity = kDefaultCapacity)
        : min_level_(static_cast<int>(Level::Info)),
          mode_(mode),
          queue_(capacity < 2 ? 2 : capacity),
          stop_(false) {
        sinks_.emplace_back(std::make_unique<ConsoleSink>()); // default sink
        if (mode_ == Mode::Async)
            worker_ = std::thread([this] { run_async(); });
    }

    ~Logger() {
        if (mode_ == Mode::Async) {
            {
                std::lock_guard<std::mutex> lk(qmutex_);
                stop_ = true;
            }
            not_empty_.notify_all();
            not_full_.notify_all();
            if (worker_.joinable()) worker_.join();
        }
        std::lock_guard<std::mutex> lk(sink_mutex_);
        for (auto& s : sinks_) s->flush();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger& set_level(Level lvl) {
        min_level_.store(static_cast<int>(lvl), std::memory_order_relaxed);
        return *this;
    }
    Level level() const {
        return static_cast<Level>(min_level_.load(std::memory_order_relaxed));
    }

    Logger& add_sink(std::unique_ptr<Sink> sink) {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        sinks_.emplace_back(std::move(sink));
        return *this;
    }
    // Remove all sinks, including the default console sink (e.g. to log only
    // to a file). Call before you start logging.
    Logger& clear_sinks() {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        sinks_.clear();
        return *this;
    }

    // Chainable log entries. Each returns a fresh LogRecord that emits when it
    // is destroyed (i.e. at the end of the statement).
    LogRecord trace(const std::string& event)    { return make_record(Level::Trace, event); }
    LogRecord debug(const std::string& event)    { return make_record(Level::Debug, event); }
    LogRecord info(const std::string& event)     { return make_record(Level::Info, event); }
    LogRecord warning(const std::string& event)  { return make_record(Level::Warning, event); }
    LogRecord warn(const std::string& event)     { return make_record(Level::Warning, event); }
    LogRecord error(const std::string& event)    { return make_record(Level::Error, event); }
    LogRecord critical(const std::string& event) { return make_record(Level::Critical, event); }

    // Block until every entry submitted so far has been written to all sinks,
    // then flush the sinks. No-op (apart from sink flush) in sync mode.
    void flush() {
        if (mode_ == Mode::Async) {
            std::unique_lock<std::mutex> lk(qmutex_);
            drained_.wait(lk, [this] { return pending_ == 0; });
        }
        std::lock_guard<std::mutex> lk(sink_mutex_);
        for (auto& s : sinks_) s->flush();
    }

    static constexpr std::size_t kDefaultCapacity = 1024;

private:
    friend class LogRecord;
    using Entry = LogRecord::Entry;

    LogRecord make_record(Level lvl, const std::string& event) {
        bool active = static_cast<int>(lvl) >= min_level_.load(std::memory_order_relaxed)
                      && lvl != Level::None;
        return LogRecord(this, lvl, event, active);
    }

    // Called by LogRecord's destructor. Sync: emit inline. Async: enqueue with
    // lossless backpressure (block the producer if the queue is full).
    void submit(Entry&& entry) {
        if (mode_ == Mode::Sync) {
            emit_entry(entry);
            return;
        }
        {
            std::unique_lock<std::mutex> lk(qmutex_);
            while (!queue_.push(entry)) {
                if (stop_) return;       // logger is shutting down; drop late entry
                not_full_.wait(lk);      // wait for the worker to free a slot
            }
            ++pending_;
        }
        not_empty_.notify_one();
    }

    void run_async() {
        for (;;) {
            std::optional<Entry> item;
            {
                std::unique_lock<std::mutex> lk(qmutex_);
                not_empty_.wait(lk, [this] { return stop_ || !queue_.empty(); });
                if (queue_.empty()) break;   // implies stop_: queue fully drained
                item = queue_.pop();         // non-empty under lock => always a value
            }
            not_full_.notify_one();
            emit_entry(*item);
            {
                std::lock_guard<std::mutex> lk(qmutex_);
                if (--pending_ == 0) drained_.notify_all();
            }
        }
    }

    void emit_entry(const Entry& entry) {
        std::string line = serialize(entry);
        std::lock_guard<std::mutex> lk(sink_mutex_);
        for (auto& s : sinks_) s->log(line);
    }

    static std::string serialize(const Entry& entry) {
        std::string line;
        line.reserve(96 + entry.event.size() + entry.fields.size() * 32);
        line += "{\"ts\":\"";
        format_timestamp(entry.time, line);
        line += "\",\"level\":\"";
        line += level_name(entry.level);
        line += "\",\"event\":\"";
        LogRecord::escape_json(entry.event, line);
        line += "\"";
        for (const auto& f : entry.fields) {
            line += ",\"";
            LogRecord::escape_json(f.first, line);
            line += "\":";
            line += f.second; // pre-encoded value (quoted string or bare number/bool)
        }
        line += "}";
        return line;
    }

    // ISO-8601 UTC with millisecond precision, e.g. 2026-06-08T21:04:05.123Z
    static void format_timestamp(std::chrono::system_clock::time_point tp, std::string& out) {
        using namespace std::chrono;
        std::time_t t = system_clock::to_time_t(tp);
        auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count() % 1000;
        if (ms < 0) ms += 1000;
        std::tm tmv{};
#if defined(_WIN32)
        gmtime_s(&tmv, &t);
#else
        gmtime_r(&t, &tmv);
#endif
        char buf[32];
        int n = std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                              tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                              tmv.tm_hour, tmv.tm_min, tmv.tm_sec, static_cast<int>(ms));
        if (n > 0) out.append(buf, static_cast<std::size_t>(n));
    }

    std::vector<std::unique_ptr<Sink>> sinks_;
    std::atomic<int> min_level_;
    Mode mode_;

    // Async machinery: a bounded queue drained by a single background worker.
    SPSCRingBuffer<Entry> queue_;
    std::mutex qmutex_;                 // guards queue_, stop_, pending_
    std::condition_variable not_empty_; // worker waits for entries
    std::condition_variable not_full_;  // producers wait for free slots (backpressure)
    std::condition_variable drained_;   // flush() waits for pending_ == 0
    std::mutex sink_mutex_;             // serializes writes to sinks_
    std::thread worker_;
    bool stop_;
    std::size_t pending_ = 0;           // entries enqueued but not yet emitted
};

inline LogRecord::LogRecord(Logger* logger, Level level, std::string event, bool active)
    : logger_(logger), active_(active) {
    if (active_) {
        entry_.level = level;
        entry_.event = std::move(event);
        entry_.time = std::chrono::system_clock::now();
    }
}

inline LogRecord::LogRecord(LogRecord&& other) noexcept
    : logger_(other.logger_), entry_(std::move(other.entry_)), active_(other.active_) {
    other.active_ = false;
    other.logger_ = nullptr;
}

inline LogRecord::~LogRecord() {
    if (active_ && logger_) logger_->submit(std::move(entry_));
}

} // namespace c_log
