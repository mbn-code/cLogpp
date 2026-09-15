# cLog++ API reference

Everything lives in namespace `c_log` (alias: `clogpp`). Include
`<clogpp/clogpp.hpp>` for all of it, or the individual headers listed with each
section.

## Levels (`clogpp/core.hpp`)

```cpp
enum class Level { Trace, Debug, Info, Warning, Error, Critical, None };
const char* level_name(Level);              // "trace" ... "critical", "none"
const char* level_label(Level);             // "TRACE   " ... padded to 8 columns
std::optional<Level> parse_level(std::string_view);
```

`parse_level` is case-insensitive and accepts `warn`, `err`, `fatal`
(Critical) and `off` (None) as aliases. `None` is a filter setting that
silences everything; it is never emitted.

## Logger (`clogpp/logger.hpp`)

```cpp
class Logger {
public:
    enum class Mode { Async, Sync };
    struct Options {
        Mode mode = Mode::Async;
        std::size_t capacity = 1024;          // async queue slots
        Level level = Level::Info;
        OverflowPolicy overflow = OverflowPolicy::Block;
        bool console = true;                  // install the default stderr sink
        bool source_location = false;         // emit file/line/func
        bool thread_id = false;               // emit tid
        std::size_t batch_size = 64;          // async: entries per drain
    };

    Logger();                                             // Options{}
    explicit Logger(Mode mode, std::size_t capacity = 1024);
    explicit Logger(const Options&);
    ~Logger();                                            // drains, joins, flushes
```

### Levels

| Method                                               | Effect                                                                                         |
| ---------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| `set_level(Level)`                                   | Minimum level for every sink. Thread-safe.                                                     |
| `level()`                                            | Current minimum.                                                                               |
| `enabled(Level)`                                     | Would an entry at this level be emitted?                                                       |
| `set_level_from_env(const char* var = "CLOG_LEVEL")` | Parse the variable; returns `false` and leaves the level untouched when unset or unrecognised. |

### Logging

```cpp
LogRecord trace(std::string_view event, SourceLocation = SourceLocation::current());
LogRecord debug(...);  LogRecord info(...);  LogRecord warning(...);  LogRecord warn(...);
LogRecord error(...);  LogRecord critical(...);
LogRecord log(Level, std::string_view event, SourceLocation = SourceLocation::current());
```

Each call returns a `LogRecord` that emits when it is destroyed, normally at
the end of the statement. Chain `kv()` calls on it. A record for a filtered
level is inactive: every `kv()` is a no-op and nothing is emitted.
`record.active()` tells you which.

```cpp
class LogRecord {
    LogRecord& kv(std::string_view key, /* see Fields */ value);
    LogRecord& kv_raw(std::string_view key, std::string_view json);
    LogRecord& message(std::string_view text);   // kv("msg", text)
    bool active() const;
};
```

### Fields

`kv(key, value)` overloads, shared by `LogRecord`, `ScopedContext` and
`Logger::Binder`:

| Value type                                               | Emitted as                                               |
| -------------------------------------------------------- | -------------------------------------------------------- |
| `std::string`, `std::string_view`, `const char*`, `char` | quoted, JSON-escaped string (`nullptr` C string is `""`) |
| `bool`                                                   | `true` / `false`                                         |
| any integer type except `bool` and `char`                | bare number                                              |
| `float`, `double`                                        | bare number, `%.17g`; NaN and infinities become `null`   |
| `std::nullptr_t`                                         | `null`                                                   |
| `std::optional<T>`                                       | the value, or `null` when empty                          |
| `kv_raw(key, json)`                                      | verbatim; you guarantee it is a valid JSON value         |

Keys are JSON-escaped. Neither keys nor values are validated for duplicates.

### Bound fields

```cpp
Logger& bind(std::string_view key, V&& value);   // one field
Binder  bind();                                  // bind().kv(...).kv(...)
Logger& unbind_all();
std::size_t bound_count() const;
```

Bound fields are emitted after `event` and before context and record fields.
Each record snapshots the bindings when it is created, so a later rebind never
alters an entry already in the async queue.

### Sinks and formatting

```cpp
Logger& add_sink(std::unique_ptr<Sink>);
template <class S, class... Args> Logger& add_sink(Args&&...);   // make_unique + add
Logger& clear_sinks();
std::size_t sink_count() const;

Logger& set_formatter(std::unique_ptr<Formatter>);               // default for sinks without one
template <class F, class... Args> Logger& set_formatter(Args&&...);

Logger& enable_source_location(bool = true);
Logger& enable_thread_id(bool = true);
```

The effective threshold for a sink is the higher of the logger level and the
sink level. Configure a sink (level, formatter) before adding it.

### Flush and diagnostics

```cpp
void flush();                    // wait for every submitted entry, then flush sinks
Mode mode() const;
std::uint64_t dropped() const;   // DropNewest discards + entries arriving during shutdown
std::uint64_t sink_errors() const;
Logger& set_error_handler(std::function<void(const char*)>);
```

Exceptions thrown by a sink's `log()` or `flush()` are caught, counted, and
passed to the error handler (if any). The async worker keeps running.

## OverflowPolicy

```cpp
enum class OverflowPolicy { Block, DropNewest };
```

`Block` (default): a producer that finds the queue full waits for the worker.
No entry is ever lost. `DropNewest`: the entry is discarded and `dropped()`
incremented; the producer never waits.

## ScopedContext (`clogpp/logger.hpp`)

```cpp
class ScopedContext {
    ScopedContext();                 // pushes a scope on the current thread
    ~ScopedContext();                // pops it
    ScopedContext& kv(...);          // same overloads as LogRecord
    static std::size_t depth();      // active context fields on this thread
};
```

Fields added to a live `ScopedContext` are attached to every record logged by
the same thread, after bound fields and before record fields. Scopes nest as a
stack and must be destroyed in reverse order (which RAII guarantees). Other
threads never see them. The object is neither copyable nor movable.

## SourceLocation (`clogpp/core.hpp`)

```cpp
struct SourceLocation {
    const char* file; const char* function; int line;
    static constexpr SourceLocation current(/* builtin defaults */);
    bool valid() const;
    std::string_view file_name() const;   // basename
};
```

Captured through `__builtin_FILE()` and friends (GCC, Clang, MSVC 2019 16.6+).
The JSON formatter emits `file` as given by the compiler (use
`-ffile-prefix-map` or similar to shorten it), `line`, and `func`. The text
formatter shows `(basename:line)`.

## Formatters (`clogpp/formatters.hpp`)

```cpp
struct Formatter {
    virtual void format(const Entry&, std::string& out) const = 0;   // no trailing newline
};

class JsonFormatter : public Formatter {
    explicit JsonFormatter(TimePrecision = TimePrecision::Milliseconds);
};

class TextFormatter : public Formatter {
    struct Options {
        bool color = false;
        TimePrecision precision = TimePrecision::Milliseconds;
        bool show_thread_id = true;
        bool show_location = true;
    };
    TextFormatter();
    explicit TextFormatter(Options);
    explicit TextFormatter(bool color);
    static bool stream_supports_color(std::FILE*);   // isatty and NO_COLOR unset
};

enum class TimePrecision { Seconds, Milliseconds, Microseconds };
```

Text output: `<ts> <LEVEL padded> <event> key=value ... [tid=N] [(file:line)]`.
Values keep their JSON encoding, so strings stay quoted.

The `Logger` calls `format()` under its sink lock. Formatters have no mutable
state, so calling them directly from several threads is also safe.

## Sinks (`clogpp/sinks.hpp` and the file sink headers)

```cpp
class Sink {
    virtual void log(const std::string& line) = 0;
    virtual void flush() {}
    Sink& set_level(Level);  Level level() const;  bool accepts(Level) const;
    Sink& set_formatter(std::unique_ptr<Formatter>);
    template <class F, class... Args> Sink& set_formatter(Args&&...);
    const Formatter* formatter() const;
};
```

| Sink                                               | Header                   | Notes                                                                                                                                                                 |
| -------------------------------------------------- | ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ConsoleSink(std::FILE* = stderr)`                 | `sinks.hpp`              | Writes line + newline; `flush()` calls `fflush`.                                                                                                                      |
| `NullSink`                                         | `sinks.hpp`              | Discards.                                                                                                                                                             |
| `CallbackSink(on_line, on_flush = {})`             | `sinks.hpp`              | Forwards to callables.                                                                                                                                                |
| `MemorySink(capacity = 1024)`                      | `sinks.hpp`              | Keeps the newest lines; `lines()`, `size()`, `clear()`. Thread-safe.                                                                                                  |
| `FileSink(path, truncate = false)`                 | `file_sink.hpp`          | Append or truncate; `is_open()`, `path()`.                                                                                                                            |
| `RotatingFileSink(path, max_bytes, max_files = 3)` | `rotating_file_sink.hpp` | Rotates `path` to `path.1` ... `path.N` when the next line would exceed `max_bytes`; `0` disables. `rotations()`, `current_size()`.                                   |
| `DailyFileSink(path, max_files = 0, clock = {})`   | `daily_file_sink.hpp`    | Writes `stem_YYYY-MM-DD.ext`, switches at local midnight, creates parent directories, prunes older dated files when `max_files > 0`. `current_path()`, `rotations()`. |

File sinks never throw from `log()`; an unopenable file yields `is_open() ==
false` and lines are dropped silently. Check it after construction.

## FieldSet and Entry (`clogpp/core.hpp`)

Custom formatters read these:

```cpp
class FieldSet {
    std::size_t size() const;
    std::string_view key(std::size_t i) const;      // JSON-escaped key
    std::string_view value(std::size_t i) const;    // JSON-encoded value
    const std::string& json_fragment() const;       // ,"k":v,"k2":v2
    void add_string / add_bool / add_null / add_integer / add_double / add_raw(...);
};

struct Entry {
    std::chrono::system_clock::time_point time;
    Level level;
    std::string event;
    std::shared_ptr<const FieldSet> bound;   // may be null
    FieldSet fields;                         // context fields, then record fields
    SourceLocation location;                 // valid() only when enabled
    std::uint64_t thread_id;                 // 0 when not enabled
};
```

## Utilities

```cpp
const char* version();                        // "0.3.0"; also CLOGPP_VERSION_MAJOR/MINOR/PATCH
std::uint64_t current_thread_id();            // OS thread id where cheap, else a stable hash
namespace detail { void format_iso8601(time_point, std::string&, TimePrecision); }
```

`SPSCRingBuffer<T>` (`clogpp/spsc_ring_buffer.hpp`) is the lock-free
single-producer/single-consumer queue the logger builds on: `push` (copy or
move; returns `false` when full without consuming the argument), `pop`
(`std::optional<T>`), `empty`, `full`, `size`, `capacity`.

## Compatibility

The 0.2 headers `logger.hpp`, `file_sink.hpp`, `rotating_file_sink.hpp` and
`spsc_ring_buffer.hpp` at the top of `include/` forward to the new locations,
and every 0.2 call still compiles. Behavioural differences from 0.2 are listed
in [CHANGELOG.md](../CHANGELOG.md).
