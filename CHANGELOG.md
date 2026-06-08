# cLog++ Changelog

All notable changes are documented in this file.

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
  scope) instead of `Logger&`. Existing chained usage — `log.info("e").kv("k", v);` —
  is unchanged.
- Documentation no longer describes the async path as "lock-free"; it is a
  mutex-synchronized multi-producer queue. Benchmarks were re-measured end-to-end.

## [v0.1.0] - 2026-02-10
### Added
- First public release: async-by-default logger with background draining and JSON output.
- File/console sinks and a chainable API.
- Initial test suite and CI.
