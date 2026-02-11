# cLog

[![Build Status](https://github.com/mbn-code/cLog/actions/workflows/ci.yml/badge.svg)](https://github.com/mbn-code/cLog/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/mbn-code/cLog?style=social)](https://github.com/mbn-code/cLog/stargazers)

**Modern C++ Structured Logging Library**

> **Async by default, chainable API, no macros.**  
> Extensible sinks, structured JSON out, robust thread lifecycle, and lossless shutdown.

> [!IMPORTANT]
> If you find cLog helpful, please consider [starring the repository](https://github.com/mbn-code/cLog) or sharing feedback. Community support helps drive improvement.

## Getting Started

```cpp
#include "include/logger.hpp"
#include <memory>

int main() {
    c_log::Logger log; // Async by default, writes to console
    log.info("startup").kv("user", "alice").kv("run", 1);
}
```

> [!IMPORTANT]
> Allow the `Logger` object to remain in scope until all events are logged. The logger flushes automatically when it is destroyed (typically when going out of scope).

## Installation
Add the `include/` directory to the project's include paths. The only requirement is a compiler supporting at least C++17. No external dependencies are needed.

> [!CAUTION]
> When compiling on Windows, ensure the compiler supports at least C++17. Consult [CI status](https://github.com/mbn-code/cLog/actions) for verified environments.

## Benchmarks

The table below presents the average time (in microseconds) to log a single entry under different modes and sinks (lower is better):

| Logger       | Mode      | Threads  | Output     | Time per Log (μs) | Source                      |
|--------------|-----------|----------|------------|-------------------|-----------------------------|
| **cLog**     | sync      | 1        | File       | 0.76              | MacBook Pro (M1 Pro)        |
| **cLog**     | async     | 1        | File       | 0.23              | MacBook Pro (M1 Pro)        |
| **cLog**     | sync      | 1        | Console    | 0.63              | MacBook Pro (M1 Pro)        |
| **cLog**     | async     | 1        | Console    | 0.22              | MacBook Pro (M1 Pro)        |

> **Note:** Benchmarks were performed locally on a MacBook Pro (M1 Pro). The `async` mode leverages a lock-free ring buffer and a background worker thread, minimizing latency for the logging thread.

**Performance Context:**  
- **cLog** provides high-throughput structured logging with minimal overhead.
- By removing heavy dependencies and optimizing the JSON serialization path, cLog achieves sub-microsecond latency even in synchronous mode.
- For optimal multi-threaded performance, asynchronous mode is recommended.

---

---

## Features
- **Zero External Dependencies:** No need for `nlohmann/json` or any other library. Just standard C++17.
- **Lightweight & Fast:** Custom, zero-allocation optimized JSON serializer.
- **Asynchronous & Synchronous:** Flexible operation modes (`Logger::Mode`).
- **Chainable API:** `info().kv().kv()` style.
- **Safe:** Automatic background flushing and lossless shutdown.
- **Cross-Platform:** Works on Linux, macOS, and Windows.

> [!TIP]
> For optimal multi-threaded performance, asynchronous mode is recommended.

<details>
<summary><strong>Advanced: Custom Sink/Output Support</strong></summary>

Custom sinks can be implemented by inheriting from `c_log::Sink`:

```cpp
struct MySink : c_log::Sink {
    void log(const std::string& msg) override {
        // Custom output
    }
};
```
Add the custom sink to the logger:
```cpp
log.add_sink(std::make_unique<MySink>());
```
</details>

## Issues and Contributions
- For bugs, feature suggestions, or questions, please [open an Issue](https://github.com/mbn-code/cLog/issues).
- Star the repository if cLog is useful.
- Contribution guidelines are available in the [Contributing Guide](CONTRIBUTING.md).
- The project roadmap is listed below:

### Project Roadmap
- [x] Robust async log draining and thread lifecycle
- [x] File and console sinks
- [x] CI/test coverage (Linux/Ubuntu)
- [ ] More flexible external sink/plugin system
- [ ] Windows and Mac CI
- [x] Simple structured logging (JSON) without external deps

## License
MIT - see [LICENSE](LICENSE)

*Project status: Alpha. The API will become more stable as users provide feedback and as adoption increases.*

## About cLog

cLog was originally developed as a practical structured logging solution for modern C++. The aim is to provide a robust, easy-to-use, and high-performance logger for projects requiring structured logs and safe multithreaded operation. Community feedback, issues, and contributions are welcome and greatly appreciated.

<details>
<summary><strong>Note on AI Involvement</strong></summary>

Some portions of this project were implemented with the aid of AI language modeling tools. As a result, some aspects of the code and design may differ from conventionally developed open source tools. User reviews, suggestions, and contributions are essential to shaping the future of cLog.

</details>
