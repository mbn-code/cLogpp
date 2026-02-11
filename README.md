# cLog++

[![Build Status](https://github.com/mbn-code/cLogpp/actions/workflows/ci.yml/badge.svg)](https://github.com/mbn-code/cLogpp/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/mbn-code/cLogpp?style=social)](https://github.com/mbn-code/cLogpp/stargazers)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-lightgrey)](https://github.com/mbn-code/cLogpp)

**Zero-Dependency, High-Performance Structured Logging for Modern C++**

> **Async by default. Chainable API. JSON structured output. No macros.**  
> cLog++ is designed for developers who need robust, thread-safe logging without the bloat of external dependencies or complex build systems.

---

## Why cLog++?

- **Zero Dependencies:** No `nlohmann/json`, no Boost, no external build systems. Just drop `include/` into your project.
- **Blazing Fast:** Custom zero-allocation JSON serializer achieves **sub-microsecond** latency (see [Benchmarks](#-benchmarks)).
- **Modern API:** Clean, chainable syntax: `log.info("user.login").kv("id", 42).kv("status", "ok");`
- **Structured:** Logs are emitted as valid JSON, ready for ingestion by ELK, Splunk, or cloud monitoring tools.
- **Thread-Safe:** robust async mode with lock-free ring buffers and automatic background flushing.

---

## Quick Start

### 1. Integration
Simply copy the `include/` directory to your project.

### 2. Usage
```cpp
#include "include/logger.hpp"
#include <memory>

int main() {
    // 1. Initialize Logger (Async by default, writes to console)
    c_log::Logger log; 

    // 2. Log structured data
    log.info("server.start")
       .kv("port", 8080)
       .kv("env", "production")
       .kv("workers", 4);
       
    // 3. Chainable logging
    log.error("db.connection_failed")
       .kv("error_code", 503)
       .kv("retries", 3);

    // Logger flushes automatically when it goes out of scope!
}
```

### 3. Build
```bash
g++ -std=c++17 -O3 -I./include main.cpp -o app -pthread
./app
```

> [!IMPORTANT]
> Keep the `Logger` object in scope for the duration of your application. It handles background worker threads and ensures all logs are flushed upon destruction.

---

## Benchmarks

**cLog++ is fast.** We benchmarked it against popular alternatives on modern hardware (MacBook Pro M1 Pro).

| Logger       | Mode      | Threads  | Output     | Time per Log (μs) | Notes                       |
|--------------|-----------|----------|------------|-------------------|-----------------------------|
| **cLog++**   | **Async** | 1        | File       | **0.23 μs**       | **Fastest (Lock-free)**     |
| **cLog++**   | Sync      | 1        | File       | 0.76 μs           | Optimized Serializer        |
| **cLog++**   | Async     | 1        | Console    | 0.22 μs           | Non-blocking                |
| **cLog++**   | Sync      | 1        | Console    | 0.63 μs           | Direct Write                |

*Lower is better. Async mode leverages a lock-free ring buffer to offload I/O to a background thread.*

---

## Features

- **Asynchronous & Synchronous:** Toggle modes easily with `Logger::Mode`.
- **Safe Lifecycle:** Automatic background thread management and lossless shutdown.
- **Multiple Sinks:** Built-in Console and File sinks.
- **Custom Sinks:** Easily extensible (inherit from `c_log::Sink`).
- **Cross-Platform:** Works seamlessly on Linux, macOS, and Windows.

> [!TIP]
> For high-throughput applications (e.g., game servers, trading systems), use **Async Mode** (default) to keep your hot path blocked for less than 250 nanoseconds per log.

---

## Advanced Usage

<details>
<summary><strong>Custom Output (Sinks)</strong></summary>

You can route logs to any destination (network, database, custom file format) by creating a custom sink:

```cpp
struct NetworkSink : c_log::Sink {
    void log(const std::string& msg) override {
        // Send 'msg' to a remote server...
    }
};

// ...
log.add_sink(std::make_unique<NetworkSink>());
```
</details>

<details>
<summary><strong>Filtering Levels</strong></summary>

Control verbosity dynamically:

```cpp
log.set_level(c_log::Level::Warning); // Ignore Info/Debug/Trace
log.warn("system.low_memory"); // Logged
log.info("system.heartbeat");  // Ignored
```
</details>

---

## Contributing

We welcome contributions! Whether it's reporting a bug, suggesting a feature, or writing code.

1.  Check the [Issues](https://github.com/mbn-code/cLogpp/issues).
2.  Read the [Contributing Guide](CONTRIBUTING.md).
3.  Open a Pull Request.

**Roadmap:**
- [x] Robust async log draining
- [x] Zero-dependency JSON serializer
- [x] File and console sinks
- [x] CI/Test coverage
- [ ] Rotating file sink support
- [ ] Windows/Mac CI runners

---

## License

MIT © [cLog++ Contributors](LICENSE).

---

> If you find cLog++ useful, please consider starring the repository on GitHub! It helps the project grow.

<details>
<summary><strong>Note on AI Involvement</strong></summary>

Some portions of this project were implemented with the aid of AI language modeling tools. As a result, some aspects of the code and design may differ from conventionally developed open source tools. User reviews, suggestions, and contributions are essential to shaping the future of cLog++.

</details>
