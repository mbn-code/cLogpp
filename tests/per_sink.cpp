// Per-sink level thresholds and per-sink formatters: one logger can send
// debug JSON to a file-like sink and warnings-only text to another.
#include "test_util.hpp"

int main() {
    std::vector<std::string> all, warnings, text;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&all);

        auto warn_sink = std::make_unique<test::CaptureSink>(&warnings);
        warn_sink->set_level(c_log::Level::Warning);
        CHECK(warn_sink->level() == c_log::Level::Warning);
        CHECK(!warn_sink->accepts(c_log::Level::Info));
        CHECK(warn_sink->accepts(c_log::Level::Error));
        log.add_sink(std::move(warn_sink));

        auto text_sink = std::make_unique<test::CaptureSink>(&text);
        text_sink->set_formatter<c_log::TextFormatter>();
        CHECK(text_sink->formatter() != nullptr);
        log.add_sink(std::move(text_sink));

        log.debug("d").kv("n", 1);
        log.info("i").kv("n", 2);
        log.warn("w").kv("n", 3);
        log.error("e").kv("n", 4);
    }
    CHECK(all.size() == 4);
    CHECK(warnings.size() == 2);
    CHECK_CONTAINS(warnings[0], "\"event\":\"w\"");
    CHECK_CONTAINS(warnings[1], "\"event\":\"e\"");

    // The text sink saw every entry, formatted as text, while the others got
    // JSON from the logger's default formatter.
    CHECK(text.size() == 4);
    CHECK_CONTAINS(text[0], "DEBUG    d n=1");
    CHECK_CONTAINS(text[2], "WARNING  w n=3");
    CHECK(text[0].front() != '{');
    CHECK(all[0].front() == '{');

    // The logger level still gates everything: a sink cannot see below it.
    std::vector<std::string> low;
    {
        c_log::Logger log(test::quiet_options());
        log.set_level(c_log::Level::Info);
        auto s = std::make_unique<test::CaptureSink>(&low);
        s->set_level(c_log::Level::Trace);
        log.add_sink(std::move(s));
        log.trace("hidden");
        log.info("shown");
    }
    CHECK(low.size() == 1);
    CHECK_CONTAINS(low[0], "shown");

    // Changing the logger's default formatter affects sinks without their own.
    std::vector<std::string> swapped;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&swapped);
        log.set_formatter<c_log::TextFormatter>();
        log.info("text.now").kv("k", "v");
    }
    CHECK(swapped.size() == 1);
    CHECK_CONTAINS(swapped[0], "INFO     text.now k=\"v\"");
    return test::result();
}
