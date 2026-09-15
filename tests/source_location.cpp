// Source locations are captured without macros and emitted only when enabled.
#include "test_util.hpp"

int main() {
    std::vector<std::string> lines;
    {
        auto o = test::quiet_options();
        o.source_location = true;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&lines);

        const int expected_line = __LINE__ + 1;
        log.info("located").kv("k", 1);

        log.enable_source_location(false);
        log.info("unlocated");

        log.enable_source_location(true);
        log.error("relocated");

        CHECK(lines.size() == 3);
        CHECK_CONTAINS(lines[0], "\"k\":1,\"file\":\"");
        CHECK_CONTAINS(lines[0], "source_location.cpp\",\"line\":" + std::to_string(expected_line) +
                                     ",\"func\":\"main\"}");
        CHECK_NOT_CONTAINS(lines[1], "\"file\"");
        CHECK_NOT_CONTAINS(lines[1], "\"line\"");
        CHECK_CONTAINS(lines[2], "\"file\":\"");
    }

    // Default: off, even though the location is always captured cheaply.
    std::vector<std::string> plain;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&plain);
        log.info("x");
    }
    CHECK_NOT_CONTAINS(plain[0], "\"file\"");

    // An explicit location can be passed through, e.g. from a wrapper.
    std::vector<std::string> explicit_lines;
    {
        auto o = test::quiet_options();
        o.source_location = true;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&explicit_lines);
        log.warn("wrapped", c_log::SourceLocation{"lib/wrapper.cpp", "wrap", 99});
    }
    CHECK_CONTAINS(explicit_lines[0],
                   "\"file\":\"lib/wrapper.cpp\",\"line\":99,\"func\":\"wrap\"}");

    // SourceLocation::file_name strips directories with either separator.
    CHECK((c_log::SourceLocation{"a/b/c.cpp", nullptr, 1}.file_name() == "c.cpp"));
    CHECK((c_log::SourceLocation{"a\\b\\c.cpp", nullptr, 1}.file_name() == "c.cpp"));
    CHECK((c_log::SourceLocation{"c.cpp", nullptr, 1}.file_name() == "c.cpp"));
    CHECK(c_log::SourceLocation{}.file_name().empty());
    CHECK(!c_log::SourceLocation{}.valid());
    CHECK(c_log::SourceLocation::current().valid() == (CLOGPP_HAS_BUILTIN_LOCATION != 0));
    return test::result();
}
