// Async mode: the destructor drains everything, and flush() blocks until every
// submitted entry has reached the sinks, even when the sink is slow.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#include "test_util.hpp"

struct SlowCountSink : c_log::Sink {
    std::atomic<int>* n;
    explicit SlowCountSink(std::atomic<int>* c) : n(c) {}
    void log(const std::string&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        n->fetch_add(1, std::memory_order_relaxed);
    }
};

int main() {
    // 1. Destructor drains: no sleep between log and scope exit.
    const char* fname = "test_async_flush.log";
    std::remove(fname);
    {
        c_log::Logger log;  // default async
        log.add_sink<c_log::FileSink>(fname);
        log.info("asyncflush").kv("payload", 12345);
    }
    {
        std::ifstream in(fname);
        std::string line;
        std::getline(in, line);
        CHECK_CONTAINS(line, "asyncflush");
        CHECK_CONTAINS(line, "12345");
    }
    std::remove(fname);

    // 2. flush() waits for delivery through a slow sink.
    std::atomic<int> count{0};
    {
        c_log::Logger log(test::quiet_options(c_log::Logger::Mode::Async));
        log.add_sink<SlowCountSink>(&count);
        const int N = 25;
        for (int i = 0; i < N; ++i) log.info("slow").kv("i", i);
        log.flush();
        CHECK(count.load() == N);  // every entry delivered before flush() returned
        log.info("after.flush");
        log.flush();
        CHECK(count.load() == N + 1);
    }
    return test::result();
}
