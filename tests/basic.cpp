// Smoke test: the default (async, console) logger constructs, logs and shuts
// down cleanly, and the version constants are consistent.
#include "test_util.hpp"

int main() {
    {
        c_log::Logger log;
        log.info("test.event").kv("a", 123);
        CHECK(log.mode() == c_log::Logger::Mode::Async);
        CHECK(log.sink_count() == 1);
        CHECK(log.level() == c_log::Level::Info);
        CHECK(log.dropped() == 0);
    }
    CHECK(std::string(c_log::version()) == CLOGPP_VERSION_STRING);
    CHECK(std::string(c_log::version()) == std::to_string(CLOGPP_VERSION_MAJOR) + "." +
                                               std::to_string(CLOGPP_VERSION_MINOR) + "." +
                                               std::to_string(CLOGPP_VERSION_PATCH));
    // The namespace alias works.
    clogpp::Logger alias(clogpp::Logger::Mode::Sync);
    alias.clear_sinks();
    alias.info("alias");
    return test::result();
}
