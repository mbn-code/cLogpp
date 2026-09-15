#pragma once
// cLog++: structured logging for modern C++.
// Async by default, chainable API, zero macros, extensible sinks, JSON output.
// License: MIT

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "clogpp/core.hpp"
#include "clogpp/formatters.hpp"
#include "clogpp/sinks.hpp"
#include "clogpp/spsc_ring_buffer.hpp"

namespace c_log {

// What to do when the async queue is full.
enum class OverflowPolicy {
    Block,      // wait for the worker to free a slot (lossless, default)
    DropNewest  // discard the new entry and count it in Logger::dropped()
};

// ---------------------------------------------------------------------------
// FieldBuilder: the chainable kv() API, shared by records, scoped context and
// logger-wide bindings. Derived supplies `FieldSet* target()`; a null target
// makes every call a cheap no-op (used for filtered-out records).
// ---------------------------------------------------------------------------
template <typename Derived>
class FieldBuilder {
public:
    Derived& kv(std::string_view key, std::string_view val) {
        if (FieldSet* f = self().target()) f->add_string(key, val);
        return self();
    }
    Derived& kv(std::string_view key, const std::string& val) {
        return kv(key, std::string_view(val));
    }
    Derived& kv(std::string_view key, const char* val) {
        return kv(key, val ? std::string_view(val) : std::string_view());
    }
    Derived& kv(std::string_view key, char val) { return kv(key, std::string_view(&val, 1)); }
    Derived& kv(std::string_view key, bool val) {
        if (FieldSet* f = self().target()) f->add_bool(key, val);
        return self();
    }
    Derived& kv(std::string_view key, std::nullptr_t) {
        if (FieldSet* f = self().target()) f->add_null(key);
        return self();
    }
    // Any integer type (short, long long, size_t, int8_t, ...) is emitted as a
    // native JSON number.
    template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> &&
                                               !std::is_same_v<T, char>,
                                           int> = 0>
    Derived& kv(std::string_view key, T val) {
        if (FieldSet* f = self().target()) f->add_integer(key, val);
        return self();
    }
    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    Derived& kv(std::string_view key, T val) {
        if (FieldSet* f = self().target()) f->add_double(key, static_cast<double>(val));
        return self();
    }
    // Optional values: absent -> JSON null.
    template <typename T>
    Derived& kv(std::string_view key, const std::optional<T>& val) {
        return val ? kv(key, *val) : kv(key, nullptr);
    }
    // Pre-encoded JSON (object, array, ...). Emitted verbatim, not validated.
    Derived& kv_raw(std::string_view key, std::string_view json) {
        if (FieldSet* f = self().target()) f->add_raw(key, json);
        return self();
    }

private:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
};

// ---------------------------------------------------------------------------
// ScopedContext: fields attached to every record logged by the current thread
// while the object is alive. Nest freely; each scope pops its own fields.
//
//   c_log::ScopedContext ctx;
//   ctx.kv("request_id", id);
//   log.info("db.query");        // carries request_id
// ---------------------------------------------------------------------------
//
// The storage lives inside the outermost ScopedContext on each thread, and a
// thread_local pointer refers to it. Only trivially-destructible thread_local
// state is used on purpose: some toolchains (MinGW-w64 emutls) free a
// thread's TLS block before running C++ thread_local destructors, which turns
// a `thread_local FieldSet` into a use-after-free at thread exit.
namespace detail {
using ThreadContextSlot = ThreadSlot<FieldSet>;
inline const FieldSet* thread_context() noexcept { return ThreadContextSlot::get(); }
}  // namespace detail

class ScopedContext : public FieldBuilder<ScopedContext> {
public:
    ScopedContext() {
        FieldSet* current = detail::ThreadContextSlot::get();
        if (current == nullptr) {
            storage_.emplace();
            detail::ThreadContextSlot::set(&*storage_);
        } else {
            fields_mark_ = current->size();
            bytes_mark_ = current->byte_size();
        }
    }
    ~ScopedContext() {
        if (storage_) {
            detail::ThreadContextSlot::set(nullptr);  // outermost scope: context ends here
        } else if (FieldSet* current = detail::ThreadContextSlot::get()) {
            current->truncate(fields_mark_, bytes_mark_);
        }
    }

    ScopedContext(const ScopedContext&) = delete;
    ScopedContext& operator=(const ScopedContext&) = delete;
    ScopedContext(ScopedContext&&) = delete;
    ScopedContext& operator=(ScopedContext&&) = delete;

    // Number of context fields currently active on this thread (all scopes).
    static std::size_t depth() noexcept {
        const FieldSet* ctx = detail::thread_context();
        return ctx ? ctx->size() : 0;
    }

    FieldSet* target() noexcept { return detail::ThreadContextSlot::get(); }

private:
    std::optional<FieldSet> storage_;  // engaged only in the outermost scope
    std::size_t fields_mark_ = 0;
    std::size_t bytes_mark_ = 0;
};

class Logger;

// ---------------------------------------------------------------------------
// LogRecord: a single log entry, built incrementally then emitted when it goes
// out of scope. Each statement gets its own record (returned by value from the
// level methods), so two threads logging on the same Logger never share state.
// ---------------------------------------------------------------------------
class LogRecord : public FieldBuilder<LogRecord> {
public:
    LogRecord(Logger* logger, Level level, std::string_view event, SourceLocation loc, bool active);
    ~LogRecord();

    // Movable (so the level methods can return it by value), non-copyable.
    LogRecord(LogRecord&& other) noexcept;
    LogRecord& operator=(LogRecord&&) = delete;
    LogRecord(const LogRecord&) = delete;
    LogRecord& operator=(const LogRecord&) = delete;

    // Free-form text, emitted as the "msg" field.
    LogRecord& message(std::string_view text) { return kv("msg", text); }

    // False when the record was filtered out; every kv() is then a no-op.
    bool active() const noexcept { return active_; }

    FieldSet* target() noexcept { return active_ ? &entry_.fields : nullptr; }

private:
    friend class Logger;

    Logger* logger_;
    Entry entry_;
    bool active_;
};

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------
class Logger {
public:
    enum class Mode { Async, Sync };

    static constexpr std::size_t kDefaultCapacity = 1024;

    struct Options {
        Mode mode = Mode::Async;
        std::size_t capacity = kDefaultCapacity;  // async queue slots
        Level level = Level::Info;
        OverflowPolicy overflow = OverflowPolicy::Block;
        bool console = true;           // install the default stderr ConsoleSink
        bool source_location = false;  // emit file/line/func with every entry
        bool thread_id = false;        // emit the logging thread's id
        std::size_t batch_size = 64;   // async: entries drained per lock acquisition
    };

    Logger() : Logger(Options{}) {}
    explicit Logger(Mode mode, std::size_t capacity = kDefaultCapacity)
        : Logger(with_mode(mode, capacity)) {}

    explicit Logger(const Options& opts)
        : min_level_(static_cast<int>(opts.level)),
          mode_(opts.mode),
          overflow_(opts.overflow),
          capture_location_(opts.source_location),
          capture_thread_id_(opts.thread_id),
          batch_size_(opts.batch_size == 0 ? 1 : opts.batch_size),
          formatter_(std::make_unique<JsonFormatter>()),
          queue_(opts.capacity < 2 ? 2 : opts.capacity + 1),
          stop_(false) {
        if (opts.console) sinks_.emplace_back(std::make_unique<ConsoleSink>());
        if (mode_ == Mode::Async) worker_ = std::thread([this] { run_async(); });
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
        for (auto& s : sinks_) flush_sink(*s);
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ---- levels ----------------------------------------------------------

    Logger& set_level(Level lvl) noexcept {
        min_level_.store(static_cast<int>(lvl), std::memory_order_relaxed);
        return *this;
    }
    Level level() const noexcept {
        return static_cast<Level>(min_level_.load(std::memory_order_relaxed));
    }
    // Read the level from an environment variable (default CLOG_LEVEL), e.g.
    // CLOG_LEVEL=debug. Returns false and leaves the level unchanged when the
    // variable is unset or not a recognised level name.
    bool set_level_from_env(const char* variable = "CLOG_LEVEL") noexcept {
        const auto v = detail::get_env(variable);
        if (!v) return false;
        if (auto lvl = parse_level(*v)) {
            set_level(*lvl);
            return true;
        }
        return false;
    }
    // Would an entry at this level be emitted? Lets callers skip expensive
    // argument preparation: `if (log.enabled(Level::Debug)) { ... }`.
    bool enabled(Level lvl) const noexcept {
        return lvl != Level::None &&
               static_cast<int>(lvl) >= min_level_.load(std::memory_order_relaxed);
    }

    // ---- sinks and formatting -------------------------------------------

    Logger& add_sink(std::unique_ptr<Sink> sink) {
        if (!sink) return *this;
        std::lock_guard<std::mutex> lk(sink_mutex_);
        sinks_.emplace_back(std::move(sink));
        return *this;
    }
    // Construct and add in one call: log.add_sink<FileSink>("app.log");
    template <typename S, typename... Args>
    Logger& add_sink(Args&&... args) {
        return add_sink(std::make_unique<S>(std::forward<Args>(args)...));
    }
    // Remove all sinks, including the default console sink (e.g. to log only
    // to a file).
    Logger& clear_sinks() {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        sinks_.clear();
        return *this;
    }
    std::size_t sink_count() const {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        return sinks_.size();
    }

    // Default formatter for sinks without one of their own (JSON by default).
    Logger& set_formatter(std::unique_ptr<Formatter> f) {
        if (!f) return *this;
        std::lock_guard<std::mutex> lk(sink_mutex_);
        formatter_ = std::move(f);
        return *this;
    }
    template <typename F, typename... Args>
    Logger& set_formatter(Args&&... args) {
        return set_formatter(std::make_unique<F>(std::forward<Args>(args)...));
    }

    Logger& enable_source_location(bool on = true) noexcept {
        capture_location_.store(on, std::memory_order_relaxed);
        return *this;
    }
    Logger& enable_thread_id(bool on = true) noexcept {
        capture_thread_id_.store(on, std::memory_order_relaxed);
        return *this;
    }

    // ---- bound fields ----------------------------------------------------
    //
    // Fields attached to every entry from this logger (service name, version,
    // host...). Records snapshot the bindings when they are created, so a
    // change never tears an in-flight async entry.

    class Binder : public FieldBuilder<Binder> {
    public:
        explicit Binder(Logger& log) : log_(log), fields_(log.bound_snapshot()) {}
        ~Binder() { log_.replace_bound(std::move(fields_)); }
        Binder(const Binder&) = delete;
        Binder& operator=(const Binder&) = delete;
        FieldSet* target() noexcept { return &fields_; }

    private:
        Logger& log_;
        FieldSet fields_;
    };

    // log.bind().kv("service", "api").kv("version", 3);
    Binder bind() { return Binder(*this); }
    // log.bind("service", "api");
    template <typename V>
    Logger& bind(std::string_view key, V&& value) {
        bind().kv(key, std::forward<V>(value));
        return *this;
    }
    Logger& unbind_all() {
        replace_bound(FieldSet{});
        return *this;
    }
    std::size_t bound_count() const {
        std::lock_guard<std::mutex> lk(bound_mutex_);
        return bound_ ? bound_->size() : 0;
    }

    // ---- logging ---------------------------------------------------------
    //
    // Each call returns a fresh LogRecord that emits when it is destroyed,
    // i.e. at the end of the statement. The defaulted SourceLocation captures
    // the call site without a macro; it is only emitted when enabled.

    LogRecord trace(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Trace, event, loc);
    }
    LogRecord debug(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Debug, event, loc);
    }
    LogRecord info(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Info, event, loc);
    }
    LogRecord warning(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Warning, event, loc);
    }
    LogRecord warn(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Warning, event, loc);
    }
    LogRecord error(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Error, event, loc);
    }
    LogRecord critical(std::string_view event, SourceLocation loc = SourceLocation::current()) {
        return make_record(Level::Critical, event, loc);
    }
    LogRecord log(Level level, std::string_view event,
                  SourceLocation loc = SourceLocation::current()) {
        return make_record(level, event, loc);
    }

    // Block until every entry submitted so far has been written to all sinks,
    // then flush the sinks. In sync mode only the sink flush applies.
    void flush() {
        if (mode_ == Mode::Async) {
            std::unique_lock<std::mutex> lk(qmutex_);
            drained_.wait(lk, [this] { return pending_ == 0; });
        }
        std::lock_guard<std::mutex> lk(sink_mutex_);
        for (auto& s : sinks_) flush_sink(*s);
    }

    // ---- diagnostics -----------------------------------------------------

    Mode mode() const noexcept { return mode_; }
    // Entries discarded because the queue was full (DropNewest) or because
    // they arrived during shutdown.
    std::uint64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }
    // Exceptions thrown by sinks, which are caught so the worker survives.
    std::uint64_t sink_errors() const noexcept {
        return sink_errors_.load(std::memory_order_relaxed);
    }
    // Called (with the exception text) whenever a sink throws. Optional.
    Logger& set_error_handler(std::function<void(const char*)> handler) {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        error_handler_ = std::move(handler);
        return *this;
    }

private:
    friend class LogRecord;

    static Options with_mode(Mode mode, std::size_t capacity) {
        Options o;
        o.mode = mode;
        o.capacity = capacity;
        return o;
    }

    LogRecord make_record(Level lvl, std::string_view event, SourceLocation loc) {
        const bool active = enabled(lvl);
        return LogRecord(this, lvl, event, loc, active);
    }

    // Called by LogRecord's constructor for active records only.
    void prepare(Entry& e) {
        e.time = std::chrono::system_clock::now();
        if (!capture_location_.load(std::memory_order_relaxed)) e.location = SourceLocation{};
        if (capture_thread_id_.load(std::memory_order_relaxed)) e.thread_id = current_thread_id();
        {
            std::lock_guard<std::mutex> lk(bound_mutex_);
            e.bound = bound_;
        }
        if (const FieldSet* ctx = detail::thread_context())
            if (!ctx->empty()) e.fields.append(*ctx);
    }

    // Called by LogRecord's destructor. Sync: emit inline. Async: enqueue,
    // applying the overflow policy when the queue is full.
    void submit(Entry&& entry) {
        if (mode_ == Mode::Sync) {
            emit_entry(entry);
            return;
        }
        {
            std::unique_lock<std::mutex> lk(qmutex_);
            while (!queue_.push(std::move(entry))) {
                if (stop_ || overflow_ == OverflowPolicy::DropNewest) {
                    dropped_.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                not_full_.wait(lk);  // wait for the worker to free a slot
            }
            ++pending_;
        }
        not_empty_.notify_one();
    }

    void run_async() {
        std::vector<Entry> batch;
        batch.reserve(batch_size_);
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(qmutex_);
                not_empty_.wait(lk, [this] { return stop_ || !queue_.empty(); });
                if (queue_.empty()) break;  // implies stop_: queue fully drained
                while (batch.size() < batch_size_) {
                    std::optional<Entry> item = queue_.pop();
                    if (!item) break;
                    batch.push_back(std::move(*item));
                }
            }
            not_full_.notify_all();
            for (const Entry& e : batch) emit_entry(e);
            {
                std::lock_guard<std::mutex> lk(qmutex_);
                pending_ -= batch.size();
                if (pending_ == 0) drained_.notify_all();
            }
            batch.clear();
        }
    }

    // Format once per distinct formatter and hand the line to every sink whose
    // level admits it. Runs on the worker (async) or the caller (sync); the
    // sink mutex serialises both the sink writes and the scratch buffers.
    void emit_entry(const Entry& e) {
        std::lock_guard<std::mutex> lk(sink_mutex_);
        bool have_default = false;
        for (auto& s : sinks_) {
            if (!s->accepts(e.level)) continue;
            const std::string* line;
            if (const Formatter* f = s->formatter()) {
                sink_line_.clear();
                f->format(e, sink_line_);
                line = &sink_line_;
            } else {
                if (!have_default) {
                    default_line_.clear();
                    formatter_->format(e, default_line_);
                    have_default = true;
                }
                line = &default_line_;
            }
            try {
                s->log(*line);
            } catch (const std::exception& ex) { report_error(ex.what()); } catch (...) {
                report_error("unknown exception in sink");
            }
        }
    }

    void flush_sink(Sink& s) noexcept {
        try {
            s.flush();
        } catch (const std::exception& ex) { report_error(ex.what()); } catch (...) {
            report_error("unknown exception in sink flush");
        }
    }

    void report_error(const char* what) noexcept {
        sink_errors_.fetch_add(1, std::memory_order_relaxed);
        if (error_handler_) {
            try {
                error_handler_(what);
            } catch (...) {}
        }
    }

    FieldSet bound_snapshot() const {
        std::lock_guard<std::mutex> lk(bound_mutex_);
        return bound_ ? *bound_ : FieldSet{};
    }
    void replace_bound(FieldSet fields) {
        auto next = fields.empty() ? std::shared_ptr<const FieldSet>{}
                                   : std::make_shared<const FieldSet>(std::move(fields));
        std::lock_guard<std::mutex> lk(bound_mutex_);
        bound_ = std::move(next);
    }

    std::atomic<int> min_level_;
    Mode mode_;
    OverflowPolicy overflow_;
    std::atomic<bool> capture_location_;
    std::atomic<bool> capture_thread_id_;
    std::size_t batch_size_;

    mutable std::mutex sink_mutex_;  // serialises sinks_, formatter_, scratch lines
    std::vector<std::unique_ptr<Sink>> sinks_;
    std::unique_ptr<Formatter> formatter_;
    std::string default_line_;
    std::string sink_line_;
    std::function<void(const char*)> error_handler_;

    mutable std::mutex bound_mutex_;
    std::shared_ptr<const FieldSet> bound_;

    // Async machinery: a bounded queue drained in batches by one worker.
    SPSCRingBuffer<Entry> queue_;
    std::mutex qmutex_;                  // guards queue_, stop_, pending_
    std::condition_variable not_empty_;  // worker waits for entries
    std::condition_variable not_full_;   // producers wait for free slots
    std::condition_variable drained_;    // flush() waits for pending_ == 0
    std::thread worker_;
    bool stop_;
    std::size_t pending_ = 0;  // entries enqueued but not yet emitted

    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> sink_errors_{0};
};

inline LogRecord::LogRecord(Logger* logger, Level level, std::string_view event, SourceLocation loc,
                            bool active)
    : logger_(logger), active_(active) {
    if (active_) {
        entry_.level = level;
        entry_.event.assign(event.data(), event.size());
        entry_.location = loc;
        logger_->prepare(entry_);
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

}  // namespace c_log

// Shorter alias for the namespace: clogpp::Logger.
namespace clogpp = c_log;
