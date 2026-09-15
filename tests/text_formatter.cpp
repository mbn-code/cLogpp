// TextFormatter output shape, colour codes, precision and toggles.
#include <chrono>

#include "test_util.hpp"

static c_log::Entry sample_entry() {
    c_log::Entry e;
    // 2026-06-08T21:04:05.123456Z
    e.time = std::chrono::system_clock::time_point(std::chrono::microseconds(1780952645123456LL));
    e.level = c_log::Level::Warning;
    e.event = "disk.low";
    e.fields.add_string("mount", "/var");
    e.fields.add_integer("free_mb", 512);
    e.fields.add_bool("critical", false);
    return e;
}

int main() {
    std::string out;

    c_log::TextFormatter plain;
    plain.format(sample_entry(), out);
    CHECK(out ==
          "2026-06-08T21:04:05.123Z WARNING  disk.low mount=\"/var\" free_mb=512 critical=false");

    // Colour wraps only the level column.
    out.clear();
    c_log::TextFormatter colored(true);
    colored.format(sample_entry(), out);
    CHECK_CONTAINS(out, "\x1b[33mWARNING \x1b[0m disk.low");
    CHECK(out.rfind("2026-06-08T21:04:05.123Z ", 0) == 0);

    // Each level has a distinct colour and CRITICAL is bold.
    std::string trace, crit;
    c_log::Entry e = sample_entry();
    e.level = c_log::Level::Trace;
    colored.format(e, trace);
    e.level = c_log::Level::Critical;
    colored.format(e, crit);
    CHECK_CONTAINS(trace, "\x1b[2mTRACE   \x1b[0m");
    CHECK_CONTAINS(crit, "\x1b[1;31mCRITICAL\x1b[0m");

    // Microsecond precision, thread id and location, then both toggled off.
    c_log::TextFormatter::Options o;
    o.precision = c_log::TimePrecision::Microseconds;
    e = sample_entry();
    e.thread_id = 4242;
    e.location = c_log::SourceLocation{"src/dir/main.cpp", "main", 17};
    out.clear();
    c_log::TextFormatter(o).format(e, out);
    CHECK_CONTAINS(out, "2026-06-08T21:04:05.123456Z WARNING ");
    CHECK_CONTAINS(out, " tid=4242 (main.cpp:17)");

    o.show_thread_id = false;
    o.show_location = false;
    o.precision = c_log::TimePrecision::Seconds;
    out.clear();
    c_log::TextFormatter(o).format(e, out);
    CHECK(out ==
          "2026-06-08T21:04:05Z WARNING  disk.low mount=\"/var\" free_mb=512 critical=false");

    // Bound fields print before per-record fields.
    auto bound = std::make_shared<c_log::FieldSet>();
    bound->add_string("service", "api");
    e = sample_entry();
    e.bound = bound;
    out.clear();
    plain.format(e, out);
    CHECK_CONTAINS(out, "disk.low service=\"api\" mount=\"/var\"");

    // Through a Logger, end to end, with a sink-level formatter.
    std::vector<std::string> lines;
    {
        c_log::Logger log(test::quiet_options());
        auto s = std::make_unique<test::CaptureSink>(&lines);
        s->set_formatter<c_log::TextFormatter>();
        log.add_sink(std::move(s));
        log.info("hello").kv("n", 1).kv("s", "two");
    }
    CHECK(lines.size() == 1);
    CHECK_CONTAINS(lines[0], "Z INFO     hello n=1 s=\"two\"");
    CHECK(lines[0].size() ==
          std::string("2026-06-08T21:04:05.123Z INFO     hello n=1 s=\"two\"").size());
    return test::result();
}
