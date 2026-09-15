# cLog++

[![Build Status](https://github.com/mbn-code/cLogpp/actions/workflows/ci.yml/badge.svg)](https://github.com/mbn-code/cLogpp/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-lightgrey)](https://github.com/mbn-code/cLogpp)

**Zero-dependency, header-only structured logging for modern C++.**

Async by default, lossless under load, JSON or text output, chainable API, no
macros. cLog++ is a small, single-namespace logger for developers who want
structured, thread-safe logging without pulling in external dependencies or a
heavy build.

```cpp
#include <clogpp/clogpp.hpp>

int main() {
    c_log::Logger log;                       // async; JSON to stderr
    log.bind("service", "api");              // on every entry from this logger

    log.info("server.start").kv("port", 8080).kv("tls", true);
    log.error("db.connect").kv("host", "db-1").kv("attempt", 3).message("timeout");
}
```

```json
{"ts":"2026-09-15T18:30:04.936Z","level":"info","event":"server.start","service":"api","port":8080,"tls":true}
{"ts":"2026-09-15T18:30:04.936Z","level":"error","event":"db.connect","service":"api","host":"db-1","attempt":3,"msg":"timeout"}
```

## Why cLog++

- **Zero dependencies.** Standard library only. Drop `include/` into your
  project, use CMake, or grab the single-header build from a release.
- **Structured output.** Every line carries a UTC timestamp, a level, an event
  name and your typed fields. Numbers, booleans and null are native JSON values,
  ready for ELK, Loki, Splunk or `jq`.
- **Context that travels with the log.** Logger-wide bound fields, per-thread
  scoped context, optional source location and thread id.
- **Lossless async.** A bounded queue with backpressure; the worker drains in
  batches. Opt into dropping instead of blocking, and read the drop counter.
- **Flexible routing.** Per-sink level thresholds and per-sink formatters: JSON
  to a rotating file, coloured text to the terminal, the last 100 lines in
  memory for a crash report, all from one logger.
- **Thread-safe, tested, sanitised.** Concurrent producers on one logger are
  safe. CI runs GCC, Clang and MSVC, plus ASan, UBSan and TSan.

## Install

**CMake (recommended)**

```cmake
add_subdirectory(cLogpp)               # or FetchContent
target_link_libraries(your_app PRIVATE clogpp::clogpp)
```

`cmake --install` produces a package for `find_package(clogpp 0.3 REQUIRED)`.

**Copy the headers.** Add `include/` to your include path and compile with C++17
and threads:

```bash
g++ -std=c++17 -O2 -Iinclude main.cpp -o app -pthread
```

**Single header.** Each release ships `clogpp_single.hpp` (also produced by
`python tools/amalgamate.py`).

Include either the umbrella header `<clogpp/clogpp.hpp>` or the piece you need
(`<clogpp/logger.hpp>`, `<clogpp/file_sink.hpp>`, and so on). The pre-0.3 paths
such as `#include "logger.hpp"` still work.

## The API in two minutes

**Levels.** `trace`, `debug`, `info`, `warning` (or `warn`), `error`,
`critical`. The default minimum is `Info`. `set_level`, `enabled(level)`, and
`set_level_from_env()` (reads `CLOG_LEVEL`, accepts `warn`, `err`, `fatal`,
`off` as aliases).

**Fields.** `kv(key, value)` accepts strings, string views, C strings, `char`,
`bool`, every integer type, `float`/`double`, `std::optional<T>` (absent is
`null`) and `nullptr`. `kv_raw(key, json)` injects a pre-encoded JSON value.
`message(text)` adds a `msg` field.

**Bound fields** are attached to every entry from a logger:

```cpp
log.bind("service", "checkout").bind("version", 3);
log.bind().kv("host", "web-1").kv("canary", true);
log.unbind_all();
```

**Scoped context** rides along with everything logged by the current thread
while it is alive, and nests:

```cpp
void handle(c_log::Logger& log, int request_id) {
    c_log::ScopedContext ctx;
    ctx.kv("request_id", request_id);
    log.info("request.start");           // carries request_id
    {
        c_log::ScopedContext span;
        span.kv("span", "db.query");
        log.debug("query.run");          // carries request_id and span
    }
}
```

**Source location and thread id** are opt-in and macro-free (captured through
compiler builtins):

```cpp
c_log::Logger::Options o;
o.source_location = true;   // adds "file", "line", "func"
o.thread_id = true;         // adds "tid"
c_log::Logger log(o);
```

**Sinks and formatters.** Each sink has its own level threshold and may carry
its own formatter; otherwise it receives the logger's default (JSON).

```cpp
log.clear_sinks();
log.add_sink<c_log::RotatingFileSink>("app.log", 10 * 1024 * 1024, 5);  // JSON
log.add_sink<c_log::DailyFileSink>("logs/app.log", 7);                  // one file per day

auto console = std::make_unique<c_log::ConsoleSink>(stderr);
console->set_level(c_log::Level::Warning);
console->set_formatter<c_log::TextFormatter>(
    c_log::TextFormatter::stream_supports_color(stderr));
log.add_sink(std::move(console));
```

```text
2026-09-15T18:30:05.037Z WARNING  disk.low free_mb=512
2026-09-15T18:30:05.037Z ERROR    db.timeout ms=3000 retry=true (main.cpp:42)
```

Built-in sinks: `ConsoleSink`, `FileSink`, `RotatingFileSink` (by size),
`DailyFileSink` (by day, with pruning), `MemorySink` (last N lines),
`CallbackSink` (any callable), `NullSink`. Custom sinks implement
`c_log::Sink::log(const std::string&)`.

**Async control.** `Logger(Options)` sets the queue capacity, the overflow
policy (`Block`, the lossless default, or `DropNewest`), and the batch size.
`flush()` blocks until everything submitted so far has reached the sinks.
`dropped()` and `sink_errors()` report what went wrong; sinks that throw are
contained and reported through `set_error_handler`.

The full reference is in [docs/api.md](docs/api.md); the threading and
performance model in [docs/design.md](docs/design.md).

## Output format

| Field                  | Type   | Notes                                                         |
| ---------------------- | ------ | ------------------------------------------------------------- |
| `ts`                   | string | ISO-8601 UTC; millisecond precision by default                |
| `level`                | string | `trace` / `debug` / `info` / `warning` / `error` / `critical` |
| `event`                | string | the event name you passed                                     |
| bound                  | mixed  | logger-wide fields, in binding order                          |
| context                | mixed  | scoped-context fields, outermost scope first                  |
| your fields            | mixed  | in call order; strings quoted and escaped, others native      |
| `msg`                  | string | only when `message()` was used                                |
| `tid`                  | number | only when thread ids are enabled                              |
| `file`, `line`, `func` | mixed  | only when source locations are enabled                        |

Keys are not de-duplicated: binding the same key twice emits it twice.

## Benchmarks

`benchmarks/benchmark_logger.cpp` times 200,000 end-to-end log calls per
scenario, **including the async drain**, so async numbers reflect real delivery.
Measured on an AMD Ryzen 7 9800X3D, Windows 11, GCC 15.2 `-O3`:

| Scenario                        | Time per log (us) |
| ------------------------------- | ----------------- |
| null sink, sync                 | 0.11              |
| text formatter, null sink, sync | 0.12              |
| bound + scoped context, sync    | 0.21              |
| file sink, sync                 | 0.31              |
| null sink, async                | 0.29              |
| file sink, async                | 0.27              |
| null sink, async, 4 producers   | 0.30              |
| filtered-out `debug()`          | 0.004             |

![cLog++ benchmark results](benchmarks/benchmark.png)

For comparison, the 0.2.0 release measured 0.20 us (null, sync) and 0.73 us
(file, async) on the same machine and flags. Async mode is not about higher
single-thread throughput: it keeps serialisation and I/O off the calling
thread while guaranteeing delivery. Run the benchmark on your own hardware;
your numbers will differ.

```bash
cmake -S . -B build -DCLOGPP_BUILD_BENCHMARKS=ON && cmake --build build
./build/benchmark_logger                # writes benchmark_results.csv
python benchmarks/plot_benchmarks.py    # writes benchmark.png (needs matplotlib)
```

## Threading model

- Any number of threads may log on the same `Logger`. Each statement builds its
  own record; nothing per-entry is shared.
- Async mode: producers push into a mutex-guarded bounded queue; one worker
  thread drains it in batches and writes to the sinks. Sink writes are
  serialised, so lines are never interleaved.
- Sync mode: the calling thread formats and writes under the sink lock.
- Keep the `Logger` alive for as long as you log through it. Its destructor
  drains the queue, joins the worker and flushes every sink.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Options: `CLOGPP_BUILD_TESTS`, `CLOGPP_BUILD_EXAMPLES`, `CLOGPP_BUILD_BENCHMARKS`,
`CLOGPP_WARNINGS_AS_ERRORS`, and `CLOGPP_SANITIZE=address,undefined` or
`thread` (GCC/Clang). See [CONTRIBUTING.md](CONTRIBUTING.md).

## Contributing

Bug reports, features and code are welcome. Check the
[issues](https://github.com/mbn-code/cLogpp/issues), read the
[contributing guide](CONTRIBUTING.md), and open a pull request.

## License

MIT. See [LICENSE](LICENSE).

<details>
<summary>Note on AI involvement</summary>

Some portions of this project were implemented with the aid of AI tooling. As a
result, some aspects of the code and design may differ from conventionally
developed open-source projects. Reviews, suggestions, and contributions are very
welcome.

</details>
