# cLog

[![Build Status](https://github.com/mbn-code/cLog/actions/workflows/ci.yml/badge.svg)](https://github.com/mbn-code/cLog/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/mbn-code/cLog?style=social)](https://github.com/mbn-code/cLog/stargazers)

**Modern C++ Structured Logging Library**

> **Async by default, chainable API, no macros.**  
> Extensible sinks, structured JSON out, robust thread lifecycle, and lossless shutdown.

> [!IMPORTANT]
> Like cLog? Please consider [starring the repo](https://github.com/mbn-code/cLog) or sharing your feedback. It really helps!

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
> Let the `Logger` object live until all events are logged. It flushes automatically when the object is destroyed (which happens when it goes out of scope).

## Install
Just add `include/` to your project. No dependencies except C++17 STL and the bundled nlohmann/json.

> [!CAUTION]
> If you're compiling on Windows, make sure your compiler supports at least C++17. See [CI status](https://github.com/mbn-code/cLog/actions) for tested environments.

## Benchmarks

The following graph shows the average time (in microseconds) to log a single entry under different modes and sinks (lower is better):

<p align="center">
  <img src="./benchmarks/benchmark.png" alt="cLog benchmarks bar graph" width="500">
</p>

_Benchmark run on a modern Linux machine (100,000 logs per variant, see `benchmarks/benchmark_logger.cpp`).  
Benchmarks were performed locally on an AMD Ryzen 9 9800X3D with 32GB DDR5-6000 CL30 RAM._

**Benchmark Comparison with Other Popular Logging Libraries**

| Logger       | Mode      | Threads  | Output     | Time per Log (μs) | Logs/sec (approx)   | Source                      |
|--------------|-----------|----------|------------|-------------------|---------------------|-----------------------------|
| **cLog**     | sync      | 1        | File       | 1.03              | 970,000             | This repo, Linux, i7        |
| **cLog**     | async     | 1        | File       | 1.22              | 820,000             | This repo, Linux, i7        |
| **cLog**     | sync      | 1        | Console    | 1.11              | 900,000             | This repo, Linux, i7        |
| **spdlog**   | sync      | 1        | File       | 0.17              | 5,770,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |
| **spdlog**   | async     | 10       | File       | 0.37              | 2,700,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |
| **spdlog**   | sync      | 10       | File       | 0.60              | 1,660,000           | [spdlog README](https://github.com/gabime/spdlog#benchmarks) |

<sub>Numbers from spdlog are for Ubuntu 64-bit, i7-4770 3.4GHz. For cLog, sync/async and file/console modes were tested with 100,000 logs per variant on a modern Linux system. “Logs/sec” is approximate, calculated as 1,000,000 / μs-per-log (higher is better).</sub>

**In context:**  
- spdlog is widely recognized as one of the fastest C++ loggers, especially in minimal-formatting scenarios.
- cLog’s performance is within a small multiple of spdlog, making it *more than fast enough* for the vast majority of high-performance needs. For many applications, sub-2μs logging throughput is essentially “free.”
- Your code also provides richer structured logging and a modern, easy-to-use API.

---

---

## Features
- Async and sync modes (`Logger::Mode`)
- Safe, automatic background flushing and shutdown
- Console and file sinks out of the box
- Fully structured JSON logs
- Clean, chainable API: `info().kv().kv()`, now with `debug()`, `warn()`, `error()` and all levels
- Lossless, race-free, and cross-platform

> [!TIP]
> For the best multithreaded performance, stick with the default async mode.

<details>
<summary><strong>Advanced</strong>: Add a custom sink/output</summary>

You can write your own sink by inheriting from `c_log::Sink`:

```cpp
struct MySink : c_log::Sink {
    void log(const std::string& msg) override {
        // Custom output
    }
};
```
And then add it to the logger:
```cpp
log.add_sink(std::make_unique<MySink>());
```
</details>

## Issues and Contributing
- Please [open an Issue](https://github.com/mbn-code/cLog/issues) for bugs, feature ideas, or questions!
- Star the repo if you find it useful.
- See the [Contributing Guide](CONTRIBUTING.md).
- You can also check out what's next below:

### Project Roadmap
- [x] Robust async log draining and thread lifecycle
- [x] File and console sinks
- [x] CI/test coverage (Linux/Ubuntu)
- [ ] More flexible external sink/plugin system
- [ ] Windows and Mac CI
- [x] Simple config file support (JSON, see examples/logger.json, logger_config.hpp)

## License
MIT - see [LICENSE](LICENSE)

*Project status: Alpha. The API will become more stable as people try it out and give feedback.*

## Why cLog?
I originally built cLog for myself. After a while, I realized other might want a modern C++ logger that's simple and just works (that's the goal, anyway! If it doesn't, please [open an issue](https://github.com/mbn-code/cLog/issues)). So, I decided to share it here. If it's useful to you, that's great. PRs and issues are always welcome!

<details>
<summary><strong>Note on AI Involvement</strong></summary>

This project was originally meant for personal use and not publication. Some parts were implemented with the help of an AI language model (LLM). Because of that, the design and code may not follow the same conventions as community-driven or "clean-room" open source tools. Your reviews, suggestions, issues, and contributions are especially valued—and will help shape cLog into something better for everyone.

</details>
