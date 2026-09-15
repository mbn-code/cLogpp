// Thread ids: emitted only when enabled, stable per thread, distinct across
// threads, and matching current_thread_id().
#include <thread>

#include "test_util.hpp"

int main() {
    const std::uint64_t me = c_log::current_thread_id();
    CHECK(me != 0);
    CHECK(c_log::current_thread_id() == me);  // cached, stable

    std::uint64_t other = 0;
    std::thread([&] { other = c_log::current_thread_id(); }).join();
    CHECK(other != 0);
    CHECK(other != me);

    std::vector<std::string> lines;
    {
        auto o = test::quiet_options();
        o.thread_id = true;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&lines);
        log.info("main.thread");
        std::thread([&] { log.info("other.thread"); }).join();
        log.enable_thread_id(false);
        log.info("no.tid");
    }
    CHECK(lines.size() == 3);
    CHECK_CONTAINS(lines[0], "\"tid\":" + std::to_string(me) + "}");
    CHECK_NOT_CONTAINS(lines[1], "\"tid\":" + std::to_string(me) + "}");
    CHECK_CONTAINS(lines[1], "\"tid\":");
    CHECK_NOT_CONTAINS(lines[2], "\"tid\"");

    // Off by default.
    std::vector<std::string> plain;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&plain);
        log.info("x");
    }
    CHECK_NOT_CONTAINS(plain[0], "\"tid\"");
    return test::result();
}
