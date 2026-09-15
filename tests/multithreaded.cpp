// Many producer threads on one async Logger: every entry arrives exactly once,
// every line is intact (no interleaving or tearing), and per-thread order is
// preserved.
#include <map>
#include <thread>

#include "test_util.hpp"

int main() {
    const int threads = 8;
    const int per_thread = 5000;
    std::vector<std::string> lines;
    {
        auto o = test::quiet_options(c_log::Logger::Mode::Async);
        o.capacity = 128;  // force plenty of backpressure
        o.thread_id = true;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&lines);

        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) {
            pool.emplace_back([&log, t] {
                c_log::ScopedContext ctx;
                ctx.kv("t", t);
                for (int i = 0; i < per_thread; ++i)
                    log.info("mt").kv("i", i).kv("pad", "0123456789abcdef");
            });
        }
        for (auto& th : pool) th.join();
        CHECK(log.dropped() == 0);
    }
    CHECK(lines.size() == static_cast<std::size_t>(threads * per_thread));

    // Each line is one complete JSON object; per-thread sequence is monotonic.
    std::map<int, int> next_expected;
    int bad = 0;
    for (const std::string& l : lines) {
        if (l.empty() || l.front() != '{' || l.back() != '}' ||
            l.find("\"pad\":\"0123456789abcdef\"") == std::string::npos) {
            ++bad;
            continue;
        }
        const std::size_t tp = l.find("\"t\":");
        const std::size_t ip = l.find("\"i\":");
        if (tp == std::string::npos || ip == std::string::npos) {
            ++bad;
            continue;
        }
        const int t = std::stoi(l.substr(tp + 4));
        const int i = std::stoi(l.substr(ip + 4));
        if (next_expected[t] != i) ++bad;
        next_expected[t] = i + 1;
    }
    CHECK(bad == 0);
    CHECK(next_expected.size() == static_cast<std::size_t>(threads));
    for (const auto& kv : next_expected) CHECK(kv.second == per_thread);

    // Sync mode with concurrent producers is also safe (sink writes are
    // serialised by the logger).
    std::vector<std::string> sync_lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&sync_lines);
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&log] {
                for (int i = 0; i < 1000; ++i) log.info("sync").kv("i", i);
            });
        for (auto& th : pool) th.join();
    }
    CHECK(sync_lines.size() == static_cast<std::size_t>(threads * 1000));
    for (const std::string& l : sync_lines) CHECK(l.front() == '{' && l.back() == '}');
    return test::result();
}
