// Logger-wide bound fields: attached to every entry, ordered before per-record
// fields, replaceable, and snapshotted per record so async entries keep the
// bindings that were active when they were logged.
#include <atomic>
#include <chrono>
#include <thread>

#include "test_util.hpp"

int main() {
    std::vector<std::string> lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&lines);
        CHECK(log.bound_count() == 0);

        log.bind("service", "api").bind("version", 3);
        CHECK(log.bound_count() == 2);
        log.info("one").kv("k", "v");

        log.bind().kv("host", "web-1").kv("canary", true);
        CHECK(log.bound_count() == 4);
        log.info("two");

        log.unbind_all();
        CHECK(log.bound_count() == 0);
        log.info("three");
    }
    CHECK(lines.size() == 3);
    CHECK_CONTAINS(lines[0], "\"event\":\"one\",\"service\":\"api\",\"version\":3,\"k\":\"v\"}");
    CHECK_CONTAINS(lines[1],
                   "\"service\":\"api\",\"version\":3,\"host\":\"web-1\",\"canary\":true}");
    CHECK_NOT_CONTAINS(lines[2], "service");

    // Async: a record logged before a rebind is emitted with the old binding
    // even though the worker formats it later.
    std::vector<std::string> async_lines;
    {
        c_log::Logger log(test::quiet_options(c_log::Logger::Mode::Async));
        log.add_sink<test::CaptureSink>(&async_lines);
        log.bind("gen", 1);
        for (int i = 0; i < 200; ++i) log.info("a").kv("i", i);
        log.bind().kv("gen", 2);  // adds a second "gen" key; the first stays first
        log.info("b");
    }
    CHECK(async_lines.size() == 201);
    for (int i = 0; i < 200; ++i) {
        CHECK_CONTAINS(async_lines[static_cast<std::size_t>(i)], "\"gen\":1,\"i\":");
        CHECK_NOT_CONTAINS(async_lines[static_cast<std::size_t>(i)], "\"gen\":2");
    }
    CHECK_CONTAINS(async_lines[200], "\"gen\":1,\"gen\":2}");
    return test::result();
}
