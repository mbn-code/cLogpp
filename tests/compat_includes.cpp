// The pre-0.3 header paths and the pre-0.3 API keep working unchanged. This
// file deliberately uses only what existed in 0.2: Logger(Mode), add_sink,
// clear_sinks, set_level, chained kv() with the old overload set, flush().
#include <cassert>
#include <cstdio>
#include <fstream>

#include "file_sink.hpp"
#include "logger.hpp"
#include "logger_debug.hpp"
#include "rotating_file_sink.hpp"
#include "spsc_ring_buffer.hpp"

struct OldStyleSink : c_log::Sink {
    int n = 0;
    void log(const std::string& message) override {
        ++n;
        last = message;
    }
    std::string last;
};

int main() {
    int failures = 0;
    const char* fname = "compat_test.log";
    std::remove(fname);
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        auto sink = std::make_unique<OldStyleSink>();
        OldStyleSink* raw = sink.get();
        log.add_sink(std::move(sink));
        log.add_sink(std::make_unique<c_log::FileSink>(fname));
        log.add_sink(std::make_unique<c_log::RotatingFileSink>("compat_rotate.log", 0, 1));
        log.set_level(c_log::Level::Debug);

        std::string s = "str";
        log.info("compat.event")
            .kv("s", s)
            .kv("c", "cstr")
            .kv("b", true)
            .kv("i", 1)
            .kv("l", 2L)
            .kv("ll", 3LL)
            .kv("u", 4u)
            .kv("ul", 5ul)
            .kv("ull", 6ull)
            .kv("d", 7.5);
        log.warn("w");
        log.flush();
        if (raw->n != 2) ++failures;
        if (raw->last.find("\"event\":\"w\"") == std::string::npos) ++failures;
        c_log::debug_log("still here");
    }
    {
        std::ifstream in(fname);
        std::string line;
        std::getline(in, line);
        if (line.find("\"s\":\"str\",\"c\":\"cstr\",\"b\":true,\"i\":1,\"l\":2,\"ll\":3,\"u\":4,"
                      "\"ul\":5,\"ull\":6,\"d\":7.5}") == std::string::npos)
            ++failures;
    }
    std::remove(fname);
    std::remove("compat_rotate.log");

    c_log::SPSCRingBuffer<int> rb(4);
    rb.push(1);
    if (rb.pop() != 1) ++failures;

    if (failures) std::fprintf(stderr, "%d compat check(s) failed\n", failures);
    return failures ? 1 : 0;
}
