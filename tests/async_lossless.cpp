// Regression test: async mode must deliver every entry, even when the producer
// far outruns the background worker (more entries than the queue can hold).
// This is the bug where ~99% of logs were silently dropped.
#include <atomic>

#include "test_util.hpp"

struct CountSink : c_log::Sink {
    std::atomic<int>* n;
    explicit CountSink(std::atomic<int>* c) : n(c) {}
    void log(const std::string&) override { n->fetch_add(1, std::memory_order_relaxed); }
};

int main() {
    const int N = 50000;  // far larger than the default 1024-slot queue
    std::atomic<int> count{0};
    {
        c_log::Logger log(c_log::Logger::Mode::Async, 256);  // small queue on purpose
        log.clear_sinks();
        log.add_sink<CountSink>(&count);
        for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
        // destructor drains the queue fully before joining the worker
        CHECK(log.dropped() == 0);
    }
    CHECK(count.load() == N);  // lossless: every entry delivered

    // Same with a tiny batch size and a capacity below the batch size, to
    // exercise the drain loop's boundaries.
    count = 0;
    {
        auto o = test::quiet_options(c_log::Logger::Mode::Async);
        o.capacity = 2;
        o.batch_size = 8;
        c_log::Logger log(o);
        log.add_sink<CountSink>(&count);
        for (int i = 0; i < 5000; ++i) log.info("tiny");
    }
    CHECK(count.load() == 5000);
    return test::result();
}
