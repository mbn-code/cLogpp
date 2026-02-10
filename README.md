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
#include "include/logger_config.hpp"
#include <memory>

int main() {
    auto log = c_log::logger_from_config("logger.json");
    log->info("startup").kv("user", "alice").kv("run", 1);
}
```

// logger.json example:
```
{
  "mode": "async", // "sync" or "async"
  "level": "info"  // "trace", "debug", ...
}
```

> [!IMPORTANT]
> Allow the `Logger` object to remain in scope until all events are logged. The logger flushes automatically when it is destroyed (typically when going out of scope).

## Installation
Add the `include/` directory to the project's include paths. The only requirements are a compiler supporting at least C++17 and the bundled nlohmann/json. No external dependencies are needed.

> [!CAUTION]
> When compiling on Windows, ensure the compiler supports at least C++17. Consult [CI status](https://github.com/mbn-code/cLog/actions) for verified environments.

## Benchmarks

The graph below presents the average time (in microseconds) to log a single entry under different modes and sinks (lower is better):

<p align="center">
  <img src="./benchmarks/benchmark.png" alt="cLog benchmarks bar graph" width="500">
</p>

_Benchmark run on a modern Linux machine (100,000 logs per variant, see `benchmarks/benchmark_logger.cpp`).  
Benchmarks were performed locally on an AMD Ryzen 9 9800X3D with 32GB DDR5-6000 CL30 RAM._

> **Note:** These results reflect a recent optimization. All cLog logging modes are now below 0.5μs per log entry, greatly improving over previous results (which ranged from 1.0–1.2μs per log).

**Benchmark Comparison with Other Popular Logging Libraries**

| Logger       | Mode      | Threads  | Output     | Time per Log (μs) | Logs/sec (approx)   | Source                      |
|--------------|-----------|----------|------------|-------------------|---------------------|-----------------------------|
| **cLog**     | sync      | 1        | File       | 0.40              | 2,500,000           | This repo, Ryzen 9800X3D    |
| **cLog**     | async     | 1        | File       | 0.47              | 2,130,000           | This repo, Ryzen 9800X3D    |
| **cLog**     | sync      | 1        | Console    | 0.35              | 2,860,000           | This repo, Ryzen 9800X3D    |
| **cLog**     | async     | 1        | Console    | 0.41              | 2,440,000           | This repo, Ryzen 9800X3D    |
| **spdlog**   | sync      | 1        | File       | 0.17              | 5,770,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |
| **spdlog**   | async     | 10       | File       | 0.37              | 2,700,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |
| **spdlog**   | sync      | 10       | File       | 0.60              | 1,660,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |

<sub>Numbers for spdlog are for Ubuntu 64-bit, i7-4770 3.4GHz. cLog benchmarks were run with 100,000 logs per variant on a modern Linux system. 'Logs/sec' values are approximate, calculated as 1,000,000 / μs-per-log (higher is better).</sub>

**Performance Context:**  
- spdlog is recognized for leading performance in minimal-formatting settings.
- cLog offers performance within a small multiple of spdlog. For most high-throughput applications, sub-2μs throughput is suitable for demanding scenarios.
- Structured logging and a modern, expressive API are provided out of the box.

---

---

## Features
- Asynchronous and synchronous operation modes (`Logger::Mode`)
- Safe, automatic background flushing and shutdown
- Console and file sinks included
- Fully structured JSON output
- Chainable API: `info().kv().kv()` and all standard log levels (`debug()`, `warn()`, `error()`, etc.)
- Race-free, lossless, and cross-platform operation

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
- [x] Simple config file support (JSON, see examples/logger.json, logger_config.hpp)

## License
MIT - see [LICENSE](LICENSE)

*Project status: Alpha. The API will become more stable as users provide feedback and as adoption increases.*

## About cLog

cLog was originally developed as a practical structured logging solution for modern C++. The aim is to provide a robust, easy-to-use, and high-performance logger for projects requiring structured logs and safe multithreaded operation. Community feedback, issues, and contributions are welcome and greatly appreciated.

<details>
<summary><strong>Note on AI Involvement</strong></summary>

Some portions of this project were implemented with the aid of AI language modeling tools. As a result, some aspects of the code and design may differ from conventionally developed open source tools. User reviews, suggestions, and contributions are essential to shaping the future of cLog.

</details>
