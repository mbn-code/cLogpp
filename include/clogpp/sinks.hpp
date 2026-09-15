#pragma once
// Sink base class and the in-process sinks: console, null, callback, memory.
// File-backed sinks live in file_sink.hpp, rotating_file_sink.hpp and
// daily_file_sink.hpp.

#include <atomic>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "clogpp/core.hpp"
#include "clogpp/formatters.hpp"

namespace c_log {

// A sink receives one fully formatted line per log entry.
//
// Each sink has its own level threshold (default Trace, i.e. everything the
// Logger lets through) and may carry its own formatter; without one it
// receives the Logger's default (JSON) output. Configure both before the sink
// is added to a Logger; the Logger reads them under its own lock afterwards.
class Sink {
public:
    virtual ~Sink() = default;

    virtual void log(const std::string& line) = 0;
    // Flush any buffered output durably. Default is a no-op so existing custom
    // sinks keep compiling unchanged.
    virtual void flush() {}

    Sink& set_level(Level lvl) noexcept {
        level_.store(static_cast<int>(lvl), std::memory_order_relaxed);
        return *this;
    }
    Level level() const noexcept {
        return static_cast<Level>(level_.load(std::memory_order_relaxed));
    }
    bool accepts(Level lvl) const noexcept {
        return static_cast<int>(lvl) >= level_.load(std::memory_order_relaxed);
    }

    Sink& set_formatter(std::unique_ptr<Formatter> f) noexcept {
        formatter_ = std::move(f);
        return *this;
    }
    template <typename F, typename... Args>
    Sink& set_formatter(Args&&... args) {
        return set_formatter(std::make_unique<F>(std::forward<Args>(args)...));
    }
    const Formatter* formatter() const noexcept { return formatter_.get(); }

private:
    std::atomic<int> level_{static_cast<int>(Level::Trace)};
    std::unique_ptr<Formatter> formatter_;
};

// Default sink: writes to stderr (keeps stdout free for program output).
class ConsoleSink : public Sink {
public:
    explicit ConsoleSink(std::FILE* stream = stderr) noexcept : stream_(stream) {}
    void log(const std::string& line) override {
        std::fwrite(line.data(), 1, line.size(), stream_);
        std::fputc('\n', stream_);
    }
    void flush() override { std::fflush(stream_); }

private:
    std::FILE* stream_;
};

// Discards everything. Useful for benchmarks and for silencing a logger.
class NullSink : public Sink {
public:
    void log(const std::string&) override {}
};

// Forwards each line to a callable. The simplest way to route logs to a
// network client, a test harness, or a GUI widget without subclassing.
class CallbackSink : public Sink {
public:
    using Callback = std::function<void(const std::string&)>;
    explicit CallbackSink(Callback on_line, std::function<void()> on_flush = {})
        : on_line_(std::move(on_line)), on_flush_(std::move(on_flush)) {}
    void log(const std::string& line) override { on_line_(line); }
    void flush() override {
        if (on_flush_) on_flush_();
    }

private:
    Callback on_line_;
    std::function<void()> on_flush_;
};

// Keeps the most recent `capacity` lines in memory. Handy in tests and for
// attaching the last N lines to a crash report or a bug-report dialog.
class MemorySink : public Sink {
public:
    explicit MemorySink(std::size_t capacity = 1024) : capacity_(capacity == 0 ? 1 : capacity) {}

    void log(const std::string& line) override {
        std::lock_guard<std::mutex> lk(mutex_);
        if (lines_.size() == capacity_) lines_.pop_front();
        lines_.push_back(line);
    }

    // Snapshot of the retained lines, oldest first.
    std::vector<std::string> lines() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return std::vector<std::string>(lines_.begin(), lines_.end());
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return lines_.size();
    }
    void clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        lines_.clear();
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<std::string> lines_;
};

}  // namespace c_log
