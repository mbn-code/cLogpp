// Sync mode: entries are emitted immediately, in order, to every sink.
#include "test_util.hpp"

int main() {
    std::vector<std::string> a, b;
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();  // drop the default console sink
        CHECK(log.sink_count() == 0);
        log.add_sink(std::make_unique<test::CaptureSink>(&a));
        log.add_sink<test::CaptureSink>(&b);
        CHECK(log.sink_count() == 2);

        log.info("sync_test").kv("x", 17);
        // In sync mode the line is already delivered here.
        CHECK(a.size() == 1);
        CHECK(b.size() == 1);
        CHECK(a[0] == b[0]);
        CHECK_CONTAINS(a[0], "\"event\":\"sync_test\"");
        CHECK_CONTAINS(a[0], "\"x\":17");  // native number, not "17"
        CHECK_CONTAINS(a[0], "\"level\":\"info\"");

        log.warn("second");
        log.error("third");
        CHECK(a.size() == 3);
        CHECK_CONTAINS(a[1], "\"event\":\"second\"");
        CHECK_CONTAINS(a[2], "\"event\":\"third\"");
        log.flush();  // no-op apart from sink flush; must not block
    }
    return test::result();
}
