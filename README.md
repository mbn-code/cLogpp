# cLog – Modern C++ Logging Library

> **Frictionless, async-by-default, structured JSON logging for modern C++**

## Quickstart Example
```cpp
#include "logger.hpp"
int main() {
    Logger log;
    log.info("system.start").kv("version", "1.0");
}
```
**First log call launches a background worker thread for async output by default.**

---

## Features (Design Table)
| Feature                 | Approach                                                        |
|-------------------------|-----------------------------------------------------------------|
| Installation            | Single header, instant use, nlohmann/json is fully internal      |
| First Log               | Async by default—first use launches worker thread, documented up front |
| Sync Mode               | Compile-time build flag for full sync, runtime opt-in if available |
| No Macros               | Never used—tooling, refactoring, and debugging always work        |
| Structured Logs         | JSON output, no user-level JSON code                            |
| Compile-time level      | Minimal overhead when disabled; eliminates dead code             |
| Sinks                   | Console/file by default; custom/extensible sinks, not required   |
| Debug/Stack Context     | Opt-in: header, adds stacktrace/thread/context as desired        |
| Thread Info             | Included via debug header/opt-in only                            |
| Config                  | (Future) JSON/YAML—deserializes to core logger API, never magic  |
| Testing/CI              | GoogleTest + GitHub Actions, boring, predictable, cross-platform |
| Docs/Examples           | Always shows one-liner start, then advanced opt-ins              |

---

## Rationale and Values
- **Zero-magic API**
- **No macros ever**: Easy refactoring and debugging
- **Async by default, never silent**: Docs and behavior preempt confusion
- **Structured JSON output only**: No user-facing JSON APIs
- **Extensible, but extensions never required**

For more, see [docs/PHILOSOPHY.md](docs/PHILOSOPHY.md) (to be added).

---

## Async and Sync Behavior
- Async (background thread and queue) is the default. First log output launches async worker.
- Sync mode: Compile-time flag disables async for embedded/hard-realtime, or use `Logger log(Logger::Mode::Sync);` at runtime where threads are available.

---

## Sinks and Extensibility
- Output goes to stderr (JSON per line) by default
- Register file or custom sinks using `.add_sink(std::make_unique<FileSink>("file.log"));`
- All extensibility is opt-in. No required advanced config.

---

## Debugging, Stack, and Thread Context
- Include `logger_debug.hpp` to add `.with_stacktrace()`/`.with_context()`/`.with_threadinfo()` to entries.
- Zero core overhead unless debug features are requested.

---

## FAQ
- **Q: Do I ever need to touch JSON types?**
  > Never. JSON is entirely internal.

- **Q: Why async by default?**
  > For performance. You control opt-out via build or API.

---

## License
MIT. Includes vendored [nlohmann/json](https://github.com/nlohmann/json) (MIT).
