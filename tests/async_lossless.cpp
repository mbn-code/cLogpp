// Regression test: async mode must deliver every entry, even when the producer
// far outruns the background worker (more entries than the queue can hold).
// This is the bug where ~99% of logs were silently dropped.
#include "logger.hpp"
#include <atomic>
#include <cassert>
#include <string>

struct CountSink : c_log::Sink {
    std::atomic<int>* n;
    explicit CountSink(std::atomic<int>* c) : n(c) {}
    void log(const std::string&) override { n->fetch_add(1, std::memory_order_relaxed); }
};

int main() {
    const int N = 50000;            // far larger than the default 1024-slot queue
    std::atomic<int> count{0};
    {
        c_log::Logger log(c_log::Logger::Mode::Async, 256); // small queue on purpose
        log.clear_sinks();
        log.add_sink(std::make_unique<CountSink>(&count));
        for (int i = 0; i < N; ++i)
            log.info("bench").kv("i", i);
        // destructor drains the queue fully before joining the worker
    }
    assert(count.load() == N); // lossless: every entry delivered
    return 0;
}
