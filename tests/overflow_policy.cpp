// OverflowPolicy: Block never drops; DropNewest discards when the queue is
// full and reports the count.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "test_util.hpp"

// A sink that blocks until released, so the queue fills up deterministically.
struct GateSink : c_log::Sink {
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    std::atomic<int> delivered{0};
    void log(const std::string&) override {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return open; });
        delivered.fetch_add(1);
    }
    void release() {
        std::lock_guard<std::mutex> lk(m);
        open = true;
        cv.notify_all();
    }
};

int main() {
    // DropNewest: with a closed gate, the worker takes one batch and stalls;
    // everything beyond the queue capacity is dropped and counted.
    {
        auto o = test::quiet_options(c_log::Logger::Mode::Async);
        o.capacity = 16;
        o.batch_size = 1;
        o.overflow = c_log::OverflowPolicy::DropNewest;
        c_log::Logger log(o);
        auto gate = std::make_unique<GateSink>();
        GateSink* g = gate.get();
        log.add_sink(std::move(gate));

        const int N = 1000;
        for (int i = 0; i < N; ++i) log.info("burst").kv("i", i);
        CHECK(log.dropped() > 0);
        CHECK(log.dropped() < static_cast<std::uint64_t>(N));
        // At most capacity queued + one in flight survive.
        CHECK(static_cast<std::uint64_t>(N) - log.dropped() <= 17);

        g->release();
        log.flush();
        CHECK(static_cast<std::uint64_t>(g->delivered.load()) + log.dropped() ==
              static_cast<std::uint64_t>(N));
    }

    // Block (default): the producer waits and nothing is lost.
    {
        auto o = test::quiet_options(c_log::Logger::Mode::Async);
        o.capacity = 16;
        c_log::Logger log(o);
        auto gate = std::make_unique<GateSink>();
        GateSink* g = gate.get();
        log.add_sink(std::move(gate));

        std::thread opener([g] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            g->release();
        });
        const int N = 500;
        for (int i = 0; i < N; ++i) log.info("blocked").kv("i", i);  // blocks until released
        opener.join();
        log.flush();
        CHECK(log.dropped() == 0);
        CHECK(g->delivered.load() == N);
    }
    return test::result();
}
