// ISO-8601 timestamps: exact output for a known instant at every precision,
// correct behaviour across the per-thread cache boundary (second changes),
// and epoch/negative edge cases.
#include <chrono>

#include "test_util.hpp"

using namespace std::chrono;

static std::string fmt(long long micros, c_log::TimePrecision p) {
    std::string out;
    c_log::detail::format_iso8601(system_clock::time_point(microseconds(micros)), out, p);
    return out;
}

int main() {
    using c_log::TimePrecision;
    // 2026-06-08T21:04:05.123456Z
    const long long t = 1780952645123456LL;
    CHECK(fmt(t, TimePrecision::Milliseconds) == "2026-06-08T21:04:05.123Z");
    CHECK(fmt(t, TimePrecision::Microseconds) == "2026-06-08T21:04:05.123456Z");
    CHECK(fmt(t, TimePrecision::Seconds) == "2026-06-08T21:04:05Z");

    // Same second, different fraction: served from the cached prefix.
    CHECK(fmt(t + 500000, TimePrecision::Milliseconds) == "2026-06-08T21:04:05.623Z");
    // Next second: the cache must refresh.
    CHECK(fmt(t + 1000000, TimePrecision::Milliseconds) == "2026-06-08T21:04:06.123Z");
    // Going backwards in time is fine too.
    CHECK(fmt(t - 6000000, TimePrecision::Milliseconds) == "2026-06-08T21:03:59.123Z");

    // Epoch and sub-millisecond truncation (not rounding).
    CHECK(fmt(0, TimePrecision::Milliseconds) == "1970-01-01T00:00:00.000Z");
    CHECK(fmt(999, TimePrecision::Milliseconds) == "1970-01-01T00:00:00.000Z");
    CHECK(fmt(999, TimePrecision::Microseconds) == "1970-01-01T00:00:00.000999Z");
    CHECK(fmt(1999999, TimePrecision::Milliseconds) == "1970-01-01T00:00:01.999Z");

    // Leap day and year boundary.
    CHECK(fmt(1709164800000000LL, TimePrecision::Seconds) == "2024-02-29T00:00:00Z");
    CHECK(fmt(1767225599000000LL, TimePrecision::Seconds) == "2025-12-31T23:59:59Z");
    CHECK(fmt(1767225600000000LL, TimePrecision::Seconds) == "2026-01-01T00:00:00Z");

    // Before the epoch: the fraction stays positive.
    CHECK(fmt(-1, TimePrecision::Microseconds) == "1969-12-31T23:59:59.999999Z");
    CHECK(fmt(-1500000, TimePrecision::Milliseconds) == "1969-12-31T23:59:58.500Z");

    // The logger's JSON output uses the same formatter and a real clock: the
    // stamp must be within a few seconds of now and lexically well-formed.
    std::vector<std::string> lines;
    const auto before = system_clock::now();
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&lines);
        log.set_formatter<c_log::JsonFormatter>(TimePrecision::Microseconds);
        log.info("now");
    }
    const auto after = system_clock::now();
    std::string b, a;
    c_log::detail::format_iso8601(before, b, TimePrecision::Microseconds);
    c_log::detail::format_iso8601(after, a, TimePrecision::Microseconds);
    const std::string stamp = lines[0].substr(7, 27);  // {"ts":"<27 chars>"
    CHECK(stamp.size() == 27 && stamp.back() == 'Z' && stamp[10] == 'T' && stamp[19] == '.');
    CHECK(b <= stamp && stamp <= a);  // ISO-8601 sorts lexically
    return test::result();
}
