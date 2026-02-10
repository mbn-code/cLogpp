# cLog

[![Build Status](https://github.com/mbn-code/cLog/actions/workflows/ci.yml/badge.svg)](https://github.com/mbn-code/cLog/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/mbn-code/cLog?style=social)](https://github.com/mbn-code/cLog/stargazers)

**Modern C++ Structured Logging Library**

---

> **Async by default, chainable API, no macros.**  
> Extensible sinks, structured JSON out; robust thread lifecycle & lossless shutdown.

---

## ⭐️ Getting Started

```cpp
#include "include/logger.hpp"
#include "include/file_sink.hpp"

int main() {
    c_log::Logger log;
    log.add_sink(std::make_unique<c_log::FileSink>("log.json"));
    log.info("startup").kv("user", "alice").kv("run", 1);
}
```

#### Install
Just add `include/` to your project. No dependencies outside C++17 STL and bundled nlohmann/json.

---

## Features
- Async and sync modes (`Logger::Mode`)
- Automatic background flushing/shutdown
- Console and file sink out-of-the-box
- Fully structured logs (JSON)
- Clean chainable API: `info().kv().kv()`
- Robust: lossless, race-free, cross-platform tested

---

## Issues and Contributing
- Please [open an Issue](https://github.com/mbn-code/cLog/issues) for bugs, features, or questions!
- Star the repo if you find it useful ⭐
- [Contributing Guide](CONTRIBUTING.md)

---

## License
MIT - see [LICENSE](LICENSE)

---

*Project status: Alpha. API will stabilize with community use.*
