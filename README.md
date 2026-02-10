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
#include "include/file_sink.hpp"

int main() {
    c_log::Logger log;
    log.add_sink(std::make_unique<c_log::FileSink>("log.json"));
    log.info("startup").kv("user", "alice").kv("run", 1);
}
```

> [!IMPORTANT]
> Let the `Logger` object live until all events are logged. It flushes automatically when the object is destroyed (which happens when it goes out of scope).

## Install
Just add `include/` to your project. No dependencies except C++17 STL and the bundled nlohmann/json.

> [!CAUTION]
> If you're compiling on Windows, make sure your compiler supports at least C++17. See [CI status](https://github.com/mbn-code/cLog/actions) for tested environments.

## Features
- Async and sync modes (`Logger::Mode`)
- Safe, automatic background flushing and shutdown
- Console and file sinks out of the box
- Fully structured JSON logs
- Clean, chainable API: `info().kv().kv()`
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
- [ ] Simple config file support

## License
MIT - see [LICENSE](LICENSE)

*Project status: Alpha. The API will become more stable as people try it out and give feedback.*

## Why cLog?
I originally built cLog for myself. After a while, I realized other folks might want a modern C++ logger that's simple and just works (that's the goal, anyway! If it doesn't, please [open an issue](https://github.com/mbn-code/cLog/issues)). So, I decided to share it here. If it's useful to you, that's great. PRs and issues are always welcome!
