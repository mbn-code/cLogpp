// Built-in in-process sinks: NullSink, CallbackSink, MemorySink, and
// ConsoleSink writing to a FILE* of our choosing.
#include <cstdio>
#include <fstream>

#include "test_util.hpp"

int main() {
    // CallbackSink forwards lines and flushes.
    std::vector<std::string> got;
    int flushes = 0;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::CallbackSink>([&](const std::string& l) { got.push_back(l); },
                                          [&] { ++flushes; });
        log.info("cb").kv("n", 1);
        log.flush();
    }
    CHECK(got.size() == 1);
    CHECK_CONTAINS(got[0], "\"event\":\"cb\"");
    CHECK(flushes == 2);  // explicit flush + destructor

    // CallbackSink without a flush callback is fine.
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::CallbackSink>([](const std::string&) {});
        log.info("x");
        log.flush();
    }

    // MemorySink keeps the newest `capacity` lines, oldest first.
    {
        c_log::Logger log(test::quiet_options());
        auto mem = std::make_unique<c_log::MemorySink>(3);
        c_log::MemorySink* m = mem.get();
        log.add_sink(std::move(mem));
        for (int i = 0; i < 10; ++i) log.info("m").kv("i", i);
        CHECK(m->size() == 3);
        auto lines = m->lines();
        CHECK(lines.size() == 3);
        CHECK_CONTAINS(lines[0], "\"i\":7");
        CHECK_CONTAINS(lines[2], "\"i\":9");
        m->clear();
        CHECK(m->size() == 0);
        CHECK(m->lines().empty());
        log.info("after.clear");
        CHECK(m->size() == 1);
    }
    CHECK(c_log::MemorySink(0).lines().empty());  // capacity 0 is clamped to 1

    // NullSink discards, and the logger still counts nothing as dropped.
    {
        c_log::Logger log(test::quiet_options(c_log::Logger::Mode::Async));
        log.add_sink<c_log::NullSink>();
        for (int i = 0; i < 1000; ++i) log.info("null");
        log.flush();
        CHECK(log.dropped() == 0);
    }

    // ConsoleSink writes line + newline to the given stream.
    const char* fname = "test_console_sink.txt";
    {
        std::FILE* f = std::fopen(fname, "w");
        CHECK(f != nullptr);
        {
            c_log::Logger log(test::quiet_options());
            log.add_sink<c_log::ConsoleSink>(f);
            log.info("to.file").kv("ok", true);
            log.info("second");
        }
        std::fclose(f);
    }
    {
        std::ifstream in(fname);
        std::string a, b, c;
        CHECK(static_cast<bool>(std::getline(in, a)));
        CHECK(static_cast<bool>(std::getline(in, b)));
        CHECK(!std::getline(in, c));
        CHECK_CONTAINS(a, "\"event\":\"to.file\",\"ok\":true}");
        CHECK_CONTAINS(b, "\"event\":\"second\"}");
    }
    std::remove(fname);

    // add_sink(nullptr) is ignored.
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink(std::unique_ptr<c_log::Sink>());
        CHECK(log.sink_count() == 0);
    }
    return test::result();
}
