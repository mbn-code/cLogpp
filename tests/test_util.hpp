#pragma once
// Minimal test support shared by the test executables.
//
// CHECK() is used instead of assert() on purpose: CI builds Release binaries
// with NDEBUG, which would compile assert() away and make every test pass.
// A failed CHECK prints the expression and location and makes main() return
// non-zero.

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "clogpp/clogpp.hpp"

namespace test {

inline int& failures() {
    static int n = 0;
    return n;
}

inline void report(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++failures();
}

#define CHECK(expr)                                             \
    do {                                                        \
        if (!(expr)) ::test::report(#expr, __FILE__, __LINE__); \
    } while (0)

#define CHECK_CONTAINS(haystack, needle)                                                  \
    do {                                                                                  \
        const std::string _h = (haystack);                                                \
        const std::string _n = (needle);                                                  \
        if (_h.find(_n) == std::string::npos) {                                           \
            std::fprintf(stderr, "CHECK_CONTAINS failed: \"%s\" not in \"%s\" (%s:%d)\n", \
                         _n.c_str(), _h.c_str(), __FILE__, __LINE__);                     \
            ++::test::failures();                                                         \
        }                                                                                 \
    } while (0)

#define CHECK_NOT_CONTAINS(haystack, needle)                                                    \
    do {                                                                                        \
        const std::string _h = (haystack);                                                      \
        const std::string _n = (needle);                                                        \
        if (_h.find(_n) != std::string::npos) {                                                 \
            std::fprintf(stderr, "CHECK_NOT_CONTAINS failed: \"%s\" found in \"%s\" (%s:%d)\n", \
                         _n.c_str(), _h.c_str(), __FILE__, __LINE__);                           \
            ++::test::failures();                                                               \
        }                                                                                       \
    } while (0)

// Return this from main().
inline int result() {
    if (failures() != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures());
        return 1;
    }
    return 0;
}

// Captures lines into a vector owned by the test, so they outlive the Logger
// (which owns and destroys its sinks). Thread-safe.
struct CaptureSink : c_log::Sink {
    std::vector<std::string>* lines;
    explicit CaptureSink(std::vector<std::string>* v) : lines(v) {}
    void log(const std::string& msg) override {
        std::lock_guard<std::mutex> lk(mutex);
        lines->push_back(msg);
    }
    std::mutex mutex;
};

// Options for a quiet logger (no console sink, everything enabled). Tests add
// a CaptureSink and pick the mode.
inline c_log::Logger::Options quiet_options(c_log::Logger::Mode mode = c_log::Logger::Mode::Sync) {
    c_log::Logger::Options o;
    o.mode = mode;
    o.console = false;
    o.level = c_log::Level::Trace;
    return o;
}

}  // namespace test
