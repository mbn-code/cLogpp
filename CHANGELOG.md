# cLog++ Changelog

All notable changes are documented in this file.

## [v0.3.0] - 2026-09-15

The largest release so far: structured context, text output, per-sink routing,
new sinks, a faster hot path, install support, and a much larger test suite.
The 0.2 API and include paths keep working.

### Added

- **Umbrella header** `<clogpp/clogpp.hpp>` and per-feature headers under
  `include/clogpp/`. The old top-level headers forward to them.
- **Bound fields**: `log.bind("service", "api")` attaches fields to every entry.
  Records snapshot the bindings, so a rebind never alters a queued entry.
- **Scoped context**: `c_log::ScopedContext` attaches fields to everything the
  current thread logs while it is alive. Nests as a stack; other threads never
  see it.
- **Source location** (`file`, `line`, `func`) and **thread id** (`tid`),
  opt-in through `Logger::Options` or `enable_source_location()` /
  `enable_thread_id()`. Captured through compiler builtins, no macros.
- **Formatters**: `JsonFormatter` (default, now with selectable timestamp
  precision) and a new human-readable `TextFormatter` with optional ANSI
  colours and a `stream_supports_color()` helper.
- **Per-sink level thresholds and per-sink formatters**: `Sink::set_level`,
  `Sink::set_formatter`. One logger can write JSON to a file and warnings-only
  text to the terminal.
- **New sinks**: `DailyFileSink` (one file per day, local midnight rotation,
  directory creation, pruning of old files), `MemorySink` (last N lines),
  `CallbackSink` (any callable), `NullSink`.
- **`Logger::Options`** constructor with queue capacity, level, overflow
  policy, default console sink toggle, source location, thread id and async
  batch size.
- **`OverflowPolicy::DropNewest`** as an alternative to blocking, plus
  `Logger::dropped()`.
- **Sink error containment**: exceptions thrown by sinks are caught and counted
  (`sink_errors()`), and reported through `set_error_handler()`. The async
  worker survives a failing sink.
- `kv()` now accepts every integer type, `float`, `char`, `std::string_view`,
  `std::nullptr_t` and `std::optional<T>`; `kv_raw()` injects pre-encoded JSON;
  `message()` adds a `msg` field.
- `Logger::log(Level, event)`, `enabled(Level)`, `set_level_from_env()`
  (`CLOG_LEVEL`), `parse_level()`, `sink_count()`, `bound_count()`, `mode()`,
  `add_sink<T>(args...)`, `set_formatter<T>(args...)`, `unbind_all()`.
- `c_log::version()` and `CLOGPP_VERSION_*` macros; `clogpp` namespace alias.
- `RotatingFileSink::rotations()` / `current_size()`, `FileSink::path()`.
- **CMake install and package config**: `cmake --install` then
  `find_package(clogpp 0.3 REQUIRED)`; `CLOGPP_WARNINGS_AS_ERRORS` and
  `CLOGPP_SANITIZE` options; strict warnings on tests and examples.
- **Single-header distribution** via `tools/amalgamate.py`, attached to GitHub
  releases by the new release workflow.
- **Tests**: 23 test programs (up from 8) covering every feature, a strict JSON
  validator over tricky output, multi-producer stress, overflow policies,
  sanitizer-clean under ASan, UBSan and TSan. Assertions no longer rely on
  `assert()`, which the Release CI build had been compiling out.
- **CI**: GCC, Clang and MSVC matrix, sanitizer jobs, an install-and-consume
  job, a single-header compile job, and a clang-format check. New examples:
  `context`, `multi_sink`, `text_console`.
- `.clang-format`, `docs/api.md`, `docs/design.md`.

### Changed

- **Faster hot path.** Timestamps are formatted arithmetically (no `gmtime`,
  no `snprintf`), the async worker drains in batches, entries are moved rather
  than copied through the queue, and formatted lines reuse a scratch buffer.
  On the reference machine: null sink sync 0.20 -> 0.11 us, file sink async
  0.73 -> 0.27 us per entry, end to end.
- `kv()` takes `std::string_view` keys; level methods take `std::string_view`
  events.
- `Logger(Mode, capacity)`: `capacity` is now exactly the number of queued
  entries (previously one slot was reserved).
- The bundled `SPSCRingBuffer` gained move-aware `push`/`pop`, `full()`,
  `size()` and `capacity()`.
- Tests run from per-test working directories so `ctest -j` is safe.
- README and docs rewritten; benchmark expanded to ten scenarios.

### Fixed

- Entries logged from a thread that also used thread-local state could crash
  at thread exit on MinGW-w64 in an early 0.3 draft; the release avoids
  non-trivially-destructible `thread_local` objects entirely.
- `FileSink` on an unwritable path no longer silently loses the reason: check
  `is_open()`.

## [v0.2.0] - 2026-06-09

### Added

- Timestamp (`ts`, ISO-8601 UTC) and `level` fields in every JSON line.
- Native JSON typing for fields: integers, unsigned integers, `double`, and `bool`
  are emitted unquoted (e.g. `{"n":42}`); `kv` gained overloads for these types and `const char*`.
- `RotatingFileSink` (size-based rotation with a bounded number of backups).
- `Logger::flush()` to block until everything submitted so far is written, plus a
  `Sink::flush()` hook (default no-op) honored on flush and destruction.
- `clear_sinks()` to drop the default console sink, and a configurable async queue
  capacity via `Logger(Mode, capacity)`.
- CMake build (`clogpp::clogpp` interface target), `ctest` integration, and CI on
  Linux, macOS, and Windows.
- Regression tests for level filtering, async losslessness, JSON field shape, and rotation.

### Fixed

- **Level filtering now works.** `set_level()` was previously never consulted, so all
  levels were emitted regardless.
- **Async logging is now lossless.** Earlier versions ignored the queue-full signal and
  silently dropped entries under load (a tight loop could lose the large majority of its
  logs), and the shutdown drain discarded entries. The queue now applies backpressure and
  drains completely before the worker exits.
- **Concurrent logging is now safe.** Per-entry state moved onto a per-statement
  `LogRecord`, removing the shared mutable builder that races when multiple threads log
  on one `Logger`.

### Changed

- `info()`/`error()`/etc. now return a `LogRecord` (which emits when it goes out of
  scope) instead of `Logger&`. Existing chained usage, `log.info("e").kv("k", v);`,
  is unchanged.
- Documentation no longer describes the async path as "lock-free"; it is a
  mutex-synchronized multi-producer queue. Benchmarks were re-measured end-to-end.

## [v0.1.0] - 2026-02-10

### Added

- First public release: async-by-default logger with background draining and JSON output.
- File/console sinks and a chainable API.
- Initial test suite and CI.
