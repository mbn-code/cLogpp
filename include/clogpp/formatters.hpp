#pragma once
// Formatters turn an Entry into one output line. The Logger owns a default
// formatter (JSON) and any sink may carry its own, so the same entry can go to
// a file as JSON and to the terminal as coloured text.

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "clogpp/core.hpp"

#if defined(_WIN32)
#    include <io.h>
#    define CLOGPP_ISATTY _isatty
#    define CLOGPP_FILENO _fileno
#else
#    include <unistd.h>
#    define CLOGPP_ISATTY isatty
#    define CLOGPP_FILENO fileno
#endif

namespace c_log {

struct Formatter {
    virtual ~Formatter() = default;
    // Append the formatted entry to `out` (no trailing newline; sinks add it).
    virtual void format(const Entry& entry, std::string& out) const = 0;
};

// One JSON object per line:
//   {"ts":"2026-06-08T21:04:05.123Z","level":"info","event":"x",<bound>,<fields>
//    [,"tid":N][,"file":"...","line":N,"func":"..."]}
class JsonFormatter : public Formatter {
public:
    explicit JsonFormatter(TimePrecision precision = TimePrecision::Milliseconds) noexcept
        : precision_(precision) {}

    void format(const Entry& e, std::string& out) const override {
        out.reserve(out.size() + 64 + e.event.size() + e.fields.json_fragment().size() +
                    (e.bound ? e.bound->json_fragment().size() : 0));
        out += "{\"ts\":\"";
        detail::format_iso8601(e.time, out, precision_);
        out += "\",\"level\":\"";
        out += level_name(e.level);
        out += "\",\"event\":\"";
        detail::escape_json(e.event, out);
        out += '"';
        if (e.bound) out += e.bound->json_fragment();
        out += e.fields.json_fragment();
        if (e.thread_id != 0) {
            out += ",\"tid\":";
            detail::append_integer(out, e.thread_id);
        }
        if (e.location.valid()) {
            out += ",\"file\":\"";
            detail::escape_json(e.location.file, out);
            out += "\",\"line\":";
            detail::append_integer(out, e.location.line);
            if (e.location.function) {
                out += ",\"func\":\"";
                detail::escape_json(e.location.function, out);
                out += '"';
            }
        }
        out += '}';
    }

private:
    TimePrecision precision_;
};

// Human-readable single line, meant for terminals and development:
//   2026-06-08T21:04:05.123Z INFO     server.start port=8080 env="production" (main.cpp:12)
// Values keep their JSON encoding (strings stay quoted), which keeps the
// formatter allocation-free and the output unambiguous.
class TextFormatter : public Formatter {
public:
    struct Options {
        bool color = false;  // ANSI colours for the level column
        TimePrecision precision = TimePrecision::Milliseconds;
        bool show_thread_id = true;
        bool show_location = true;
    };

    TextFormatter() = default;
    explicit TextFormatter(Options opts) noexcept : opts_(opts) {}
    explicit TextFormatter(bool color) noexcept { opts_.color = color; }

    // True when `stream` is an interactive terminal (and NO_COLOR is unset), a
    // reasonable default for `Options::color`.
    static bool stream_supports_color(std::FILE* stream) noexcept {
        if (detail::get_env("NO_COLOR")) return false;
        return CLOGPP_ISATTY(CLOGPP_FILENO(stream)) != 0;
    }

    void format(const Entry& e, std::string& out) const override {
        detail::format_iso8601(e.time, out, opts_.precision);
        out += ' ';
        if (opts_.color) out += color_for(e.level);
        out += level_label(e.level);
        if (opts_.color) out += "\x1b[0m";
        out += ' ';
        out += e.event;
        if (e.bound) append_fields(*e.bound, out);
        append_fields(e.fields, out);
        if (opts_.show_thread_id && e.thread_id != 0) {
            out += " tid=";
            detail::append_integer(out, e.thread_id);
        }
        if (opts_.show_location && e.location.valid()) {
            out += " (";
            out += e.location.file_name();
            out += ':';
            detail::append_integer(out, e.location.line);
            out += ')';
        }
    }

private:
    static void append_fields(const FieldSet& fs, std::string& out) {
        for (std::size_t i = 0; i < fs.size(); ++i) {
            out += ' ';
            out += fs.key(i);
            out += '=';
            out += fs.value(i);
        }
    }

    static const char* color_for(Level lvl) noexcept {
        switch (lvl) {
            case Level::Trace: return "\x1b[2m";        // dim
            case Level::Debug: return "\x1b[36m";       // cyan
            case Level::Info: return "\x1b[32m";        // green
            case Level::Warning: return "\x1b[33m";     // yellow
            case Level::Error: return "\x1b[31m";       // red
            case Level::Critical: return "\x1b[1;31m";  // bold red
            default: return "";
        }
    }

    Options opts_{};
};

}  // namespace c_log

#undef CLOGPP_ISATTY
#undef CLOGPP_FILENO
