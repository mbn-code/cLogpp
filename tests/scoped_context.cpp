// ScopedContext: thread-local fields that ride along with every record while
// the scope is alive; nests as a stack; isolated between threads.
#include <thread>

#include "test_util.hpp"

int main() {
    std::vector<std::string> lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&lines);

        log.info("outside");
        CHECK(c_log::ScopedContext::depth() == 0);
        {
            c_log::ScopedContext req;
            req.kv("request_id", "r-1").kv("user", 42);
            CHECK(c_log::ScopedContext::depth() == 2);
            log.info("in.request").kv("step", 1);
            {
                c_log::ScopedContext inner;
                inner.kv("span", "db");
                CHECK(c_log::ScopedContext::depth() == 3);
                log.debug("in.span");
            }
            CHECK(c_log::ScopedContext::depth() == 2);
            log.info("after.span");
        }
        CHECK(c_log::ScopedContext::depth() == 0);
        log.info("outside.again");

        // Another thread does not see this thread's context.
        {
            c_log::ScopedContext mine;
            mine.kv("owner", "main");
            std::thread([&] {
                CHECK(c_log::ScopedContext::depth() == 0);
                c_log::ScopedContext theirs;
                theirs.kv("owner", "worker");
                log.info("from.thread");
            }).join();
            log.info("from.main");
        }
    }
    CHECK(lines.size() == 7);
    CHECK_NOT_CONTAINS(lines[0], "request_id");
    CHECK_CONTAINS(lines[1],
                   "\"event\":\"in.request\",\"request_id\":\"r-1\",\"user\":42,\"step\":1}");
    CHECK_CONTAINS(lines[2], "\"request_id\":\"r-1\",\"user\":42,\"span\":\"db\"}");
    CHECK_CONTAINS(lines[3], "\"request_id\":\"r-1\",\"user\":42}");
    CHECK_NOT_CONTAINS(lines[3], "\"span\":");
    CHECK_NOT_CONTAINS(lines[4], "request_id");
    CHECK_CONTAINS(lines[5], "\"owner\":\"worker\"}");
    CHECK_NOT_CONTAINS(lines[5], "main");
    CHECK_CONTAINS(lines[6], "\"owner\":\"main\"}");

    // Bound fields come before context, context before record fields.
    std::vector<std::string> order;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&order);
        log.bind("b", 1);
        c_log::ScopedContext ctx;
        ctx.kv("c", 2);
        log.info("o").kv("r", 3);
    }
    CHECK_CONTAINS(order[0], "\"event\":\"o\",\"b\":1,\"c\":2,\"r\":3}");
    return test::result();
}
