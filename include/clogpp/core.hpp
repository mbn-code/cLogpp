#pragma once
// cLog++ core types: levels, source locations, encoded field sets, log entries,
// and the small time/thread helpers shared by formatters and sinks.
//
// This header has no dependencies beyond the C++17 standard library.

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#define CLOGPP_VERSION_MAJOR  0
#define CLOGPP_VERSION_MINOR  3
#define CLOGPP_VERSION_PATCH  0
#define CLOGPP_VERSION_STRING "0.3.0"

namespace c_log {

inline constexpr const char* version() noexcept { return CLOGPP_VERSION_STRING; }

// ---------------------------------------------------------------------------
// Levels
// ---------------------------------------------------------------------------

// Log levels, ordered by severity. None disables all output.
enum class Level : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    None = 6
};

inline constexpr const char* level_name(Level lvl) noexcept {
    switch (lvl) {
        case Level::Trace: return "trace";
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warning: return "warning";
        case Level::Error: return "error";
        case Level::Critical: return "critical";
        default: return "none";
    }
}

// Upper-case, padded to 8 columns so text output lines up ("INFO    ").
inline constexpr const char* level_label(Level lvl) noexcept {
    switch (lvl) {
        case Level::Trace: return "TRACE   ";
        case Level::Debug: return "DEBUG   ";
        case Level::Info: return "INFO    ";
        case Level::Warning: return "WARNING ";
        case Level::Error: return "ERROR   ";
        case Level::Critical: return "CRITICAL";
        default: return "NONE    ";
    }
}

// Parse a level name, case-insensitively. Accepts the canonical names plus the
// common aliases "warn", "err", "fatal" (-> Critical) and "off" (-> None).
// Returns nullopt for anything else so callers can fall back explicitly.
inline std::optional<Level> parse_level(std::string_view text) noexcept {
    char buf[16];
    if (text.empty() || text.size() >= sizeof(buf)) return std::nullopt;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    std::string_view s(buf, text.size());
    if (s == "trace") return Level::Trace;
    if (s == "debug") return Level::Debug;
    if (s == "info") return Level::Info;
    if (s == "warning" || s == "warn") return Level::Warning;
    if (s == "error" || s == "err") return Level::Error;
    if (s == "critical" || s == "fatal") return Level::Critical;
    if (s == "none" || s == "off") return Level::None;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Source location (macro-free)
// ---------------------------------------------------------------------------

// Captured via compiler builtins in a defaulted argument, so `log.info("x")`
// records the caller's file/line/function without any macro. Supported by GCC,
// Clang and MSVC 2019 16.6+; elsewhere the location is simply absent.
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1926)
#    define CLOGPP_HAS_BUILTIN_LOCATION 1
#else
#    define CLOGPP_HAS_BUILTIN_LOCATION 0
#endif

struct SourceLocation {
    const char* file = nullptr;
    const char* function = nullptr;
    int line = 0;

#if CLOGPP_HAS_BUILTIN_LOCATION
    static constexpr SourceLocation current(const char* file = __builtin_FILE(),
                                            const char* function = __builtin_FUNCTION(),
                                            int line = __builtin_LINE()) noexcept {
        return SourceLocation{file, function, line};
    }
#else
    static constexpr SourceLocation current() noexcept { return SourceLocation{}; }
#endif

    constexpr bool valid() const noexcept { return file != nullptr; }

    // "src/main.cpp" -> "main.cpp" (handles both separators).
    std::string_view file_name() const noexcept {
        if (!file) return {};
        std::string_view f(file);
        std::size_t p = f.find_last_of("/\\");
        return p == std::string_view::npos ? f : f.substr(p + 1);
    }
};

// ---------------------------------------------------------------------------
// JSON encoding helpers
// ---------------------------------------------------------------------------

namespace detail {

inline void escape_json(std::string_view input, std::string& output) {
    for (char c : input) {
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
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    output += buf;
                } else {
                    output += c;
                }
        }
    }
}

template <typename T>
inline void append_integer(std::string& out, T v) {
    char buf[24];
    auto r = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, static_cast<std::size_t>(r.ptr - buf));
}

inline void append_double(std::string& out, double v) {
    if (!std::isfinite(v)) {
        out += "null";
        return;
    }  // JSON has no NaN/Infinity
    char buf[32];
    int n = std::snprintf(buf, sizeof(buf), "%.17g", v);
    if (n > 0) out.append(buf, static_cast<std::size_t>(n));
}

// Read an environment variable without tripping MSVC's getenv deprecation.
inline std::optional<std::string> get_env(const char* name) {
#if defined(_MSC_VER)
    char buf[512];
    std::size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), name) != 0 || len == 0) return std::nullopt;
    return std::string(buf, len - 1);  // len counts the terminator
#else
    const char* v = std::getenv(name);
    if (!v) return std::nullopt;
    return std::string(v);
#endif
}

}  // namespace detail

// ---------------------------------------------------------------------------
// FieldSet: an ordered list of key/value pairs, stored pre-encoded as JSON.
// ---------------------------------------------------------------------------
//
// Values are stored in their final JSON form (quoted+escaped strings, bare
// numbers/bools), laid out in one contiguous buffer as `,"key":value,...`.
// The JSON formatter appends that buffer verbatim; other formatters walk the
// spans. One string + one small vector per record, regardless of field count.
class FieldSet {
public:
    struct Span {
        std::uint32_t key_off, key_len, val_off, val_len;
    };

    FieldSet() = default;

    std::size_t size() const noexcept { return spans_.size(); }
    bool empty() const noexcept { return spans_.empty(); }

    // Key as stored (JSON-escaped).
    std::string_view key(std::size_t i) const noexcept {
        const Span& s = spans_[i];
        return std::string_view(buf_).substr(s.key_off, s.key_len);
    }
    // Value in its JSON form: `"text"`, `42`, `true`, `null`, ...
    std::string_view value(std::size_t i) const noexcept {
        const Span& s = spans_[i];
        return std::string_view(buf_).substr(s.val_off, s.val_len);
    }

    // The whole set as a JSON object fragment: `,"k":v,"k2":v2` (leading comma).
    const std::string& json_fragment() const noexcept { return buf_; }

    void clear() noexcept {
        buf_.clear();
        spans_.clear();
    }

    void reserve(std::size_t bytes, std::size_t fields = 4) {
        buf_.reserve(bytes);
        spans_.reserve(fields);
    }

    // Append everything from another set (used for scoped context).
    void append(const FieldSet& other) {
        const std::uint32_t base = static_cast<std::uint32_t>(buf_.size());
        buf_ += other.buf_;
        spans_.reserve(spans_.size() + other.spans_.size());
        for (Span s : other.spans_) {
            s.key_off += base;
            s.val_off += base;
            spans_.push_back(s);
        }
    }

    // Drop everything appended after a mark taken with size()/byte_size().
    void truncate(std::size_t fields, std::size_t bytes) {
        spans_.resize(fields);
        buf_.resize(bytes);
    }
    std::size_t byte_size() const noexcept { return buf_.size(); }

    // Typed adders. Strings are quoted and escaped; everything else is native.
    void add_string(std::string_view key, std::string_view val) {
        begin(key);
        buf_ += '"';
        detail::escape_json(val, buf_);
        buf_ += '"';
        end();
    }
    void add_bool(std::string_view key, bool val) {
        begin(key);
        buf_ += val ? "true" : "false";
        end();
    }
    void add_null(std::string_view key) {
        begin(key);
        buf_ += "null";
        end();
    }
    template <typename T>
    std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>> add_integer(
        std::string_view key, T val) {
        begin(key);
        detail::append_integer(buf_, val);
        end();
    }
    void add_double(std::string_view key, double val) {
        begin(key);
        detail::append_double(buf_, val);
        end();
    }
    // Pre-encoded JSON value (object, array, number...). Not validated: the
    // caller guarantees `json` is a well-formed JSON value.
    void add_raw(std::string_view key, std::string_view json) {
        begin(key);
        buf_ += json;
        end();
    }

private:
    void begin(std::string_view key) {
        buf_ += ",\"";
        const std::size_t key_off = buf_.size();
        detail::escape_json(key, buf_);
        const std::size_t key_len = buf_.size() - key_off;
        buf_ += "\":";
        spans_.push_back(Span{static_cast<std::uint32_t>(key_off),
                              static_cast<std::uint32_t>(key_len),
                              static_cast<std::uint32_t>(buf_.size()), 0});
    }
    void end() {
        Span& s = spans_.back();
        s.val_len = static_cast<std::uint32_t>(buf_.size() - s.val_off);
    }

    std::string buf_;
    std::vector<Span> spans_;
};

// ---------------------------------------------------------------------------
// Entry: one fully captured log record, ready to be formatted.
// ---------------------------------------------------------------------------

struct Entry {
    std::chrono::system_clock::time_point time;
    Level level = Level::Info;
    std::string event;
    // Logger-wide bound fields, snapshotted when the record was created so an
    // async entry is formatted with the bindings that were active at log time.
    std::shared_ptr<const FieldSet> bound;
    // Thread-scoped context (ScopedContext) followed by per-record fields.
    FieldSet fields;
    SourceLocation location;      // valid() only when source locations are enabled
    std::uint64_t thread_id = 0;  // 0 when thread ids are not enabled
};

// ---------------------------------------------------------------------------
// Time and thread helpers
// ---------------------------------------------------------------------------

enum class TimePrecision { Seconds, Milliseconds, Microseconds };

namespace detail {

// Format an ISO-8601 UTC timestamp, e.g. 2026-06-08T21:04:05.123Z.
//
// The civil date is computed arithmetically (Howard Hinnant's days-to-civil
// algorithm) instead of calling gmtime(), so this is a few dozen integer
// operations with no locale, no locks and no thread-local state. That last
// point matters: some toolchains (MinGW-w64) emulate thread_local with a
// lookup that costs several hundred nanoseconds per access.
inline void format_iso8601(std::chrono::system_clock::time_point tp, std::string& out,
                           TimePrecision precision = TimePrecision::Milliseconds) {
    using namespace std::chrono;
    long long us = duration_cast<microseconds>(tp.time_since_epoch()).count();
    long long secs = us / 1000000;
    long long sub = us % 1000000;
    if (sub < 0) {
        sub += 1000000;
        --secs;
    }
    long long days = secs / 86400;
    long long sod = secs % 86400;
    if (sod < 0) {
        sod += 86400;
        --days;
    }

    // days since 1970-01-01 -> civil y/m/d (proleptic Gregorian).
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const long long doe = days - era * 146097;                                    // [0, 146096]
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;  // [0, 399]
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                // [0, 365]
    const long long mp = (5 * doy + 2) / 153;                                     // [0, 11]
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);                 // [1, 31]
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);                    // [1, 12]
    int y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));

    char buf[32];
    char* p = buf;
    auto put2 = [&p](int v) {
        *p++ = static_cast<char>('0' + v / 10);
        *p++ = static_cast<char>('0' + v % 10);
    };
    if (y < 0) {
        *p++ = '-';
        y = -y;
    }
    put2(y / 100 % 100);
    put2(y % 100);
    *p++ = '-';
    put2(m);
    *p++ = '-';
    put2(d);
    *p++ = 'T';
    put2(static_cast<int>(sod / 3600));
    *p++ = ':';
    put2(static_cast<int>(sod / 60 % 60));
    *p++ = ':';
    put2(static_cast<int>(sod % 60));
    if (precision != TimePrecision::Seconds) {
        *p++ = '.';
        int v = static_cast<int>(precision == TimePrecision::Milliseconds ? sub / 1000 : sub);
        const int digits = precision == TimePrecision::Milliseconds ? 3 : 6;
        for (int i = digits - 1; i >= 0; --i) {
            p[i] = static_cast<char>('0' + v % 10);
            v /= 10;
        }
        p += digits;
    }
    *p++ = 'Z';
    out.append(buf, static_cast<std::size_t>(p - buf));
}

}  // namespace detail

}  // namespace c_log

// Platform thread helpers. The Windows functions are declared directly so
// that <windows.h> (with its macros) never enters user code.
#if defined(_WIN32)
extern "C" {
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
__declspec(dllimport) void* __stdcall TlsGetValue(unsigned long index);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long index, void* value);
}
#elif defined(__linux__)
#    include <sys/syscall.h>
#    include <unistd.h>
#elif defined(__APPLE__)
#    include <pthread.h>
#endif

namespace c_log {

// A stable per-thread numeric id: the OS thread id where it is cheap to
// obtain, otherwise a hash of std::thread::id (stable for the life of the
// thread, but not the OS id).
inline std::uint64_t current_thread_id() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentThreadId());  // reads the TEB; no cache needed
#else
    thread_local std::uint64_t cached = 0;
    if (cached == 0) {
#    if defined(__linux__)
        cached = static_cast<std::uint64_t>(::syscall(SYS_gettid));
#    elif defined(__APPLE__)
        std::uint64_t tid = 0;
        pthread_threadid_np(nullptr, &tid);
        cached = tid;
#    else
        cached =
            static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#    endif
        if (cached == 0) cached = 1;  // keep 0 reserved for "not captured"
    }
    return cached;
#endif
}

namespace detail {

// One pointer of per-thread storage, used for the scoped-context stack.
//
// On MinGW-w64, `thread_local` goes through emulated TLS and costs several
// hundred nanoseconds per access, so a native Win32 TLS slot is used there
// instead. Everywhere else `thread_local` is a couple of instructions. Only
// trivially-destructible per-thread state is involved either way.
template <typename T>
struct ThreadSlot {
#if defined(_WIN32) && defined(__GNUC__)
    static unsigned long index() noexcept {
        static const unsigned long idx = TlsAlloc();
        return idx;
    }
    static T* get() noexcept {
        const unsigned long idx = index();
        return idx == 0xFFFFFFFFul ? nullptr : static_cast<T*>(TlsGetValue(idx));
    }
    static void set(T* value) noexcept {
        const unsigned long idx = index();
        if (idx != 0xFFFFFFFFul) TlsSetValue(idx, value);
    }
#else
    static T*& slot() noexcept {
        thread_local T* value = nullptr;
        return value;
    }
    static T* get() noexcept { return slot(); }
    static void set(T* value) noexcept { slot() = value; }
#endif
};

}  // namespace detail

}  // namespace c_log
