// Level filtering, Level::None, enabled(), parse_level() and the environment
// variable override.
#include "test_util.hpp"

int main() {
    std::vector<std::string> lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&lines);

        log.set_level(c_log::Level::Warning);
        CHECK(log.level() == c_log::Level::Warning);
        CHECK(!log.enabled(c_log::Level::Info));
        CHECK(log.enabled(c_log::Level::Warning));
        CHECK(log.enabled(c_log::Level::Critical));
        CHECK(!log.enabled(c_log::Level::None));

        log.trace("t");            // filtered
        log.debug("d");            // filtered
        log.info("i").kv("x", 1);  // filtered; kv on a filtered record is a no-op
        CHECK(!log.info("probe").active());
        log.warn("w");      // kept
        log.error("e");     // kept
        log.critical("c");  // kept
        log.log(c_log::Level::Error, "generic");
    }
    CHECK(lines.size() == 4);  // only warn, error, critical, generic survive
    CHECK_CONTAINS(lines[0], "\"event\":\"w\"");
    CHECK_CONTAINS(lines[0], "\"level\":\"warning\"");
    CHECK_CONTAINS(lines[1], "\"event\":\"e\"");
    CHECK_CONTAINS(lines[2], "\"event\":\"c\"");
    CHECK_CONTAINS(lines[3], "\"event\":\"generic\"");
    CHECK_CONTAINS(lines[3], "\"level\":\"error\"");

    // Level::None silences everything, including critical.
    std::vector<std::string> none_lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&none_lines);
        log.set_level(c_log::Level::None);
        log.critical("should.not.appear");
    }
    CHECK(none_lines.empty());

    // Options::level applies from construction.
    std::vector<std::string> opt_lines;
    {
        auto o = test::quiet_options();
        o.level = c_log::Level::Error;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&opt_lines);
        log.warn("hidden");
        log.error("shown");
    }
    CHECK(opt_lines.size() == 1);

    // parse_level: canonical names, aliases, case-insensitivity, rejects junk.
    using c_log::Level;
    using c_log::parse_level;
    CHECK(parse_level("trace") == Level::Trace);
    CHECK(parse_level("DEBUG") == Level::Debug);
    CHECK(parse_level("Info") == Level::Info);
    CHECK(parse_level("warning") == Level::Warning);
    CHECK(parse_level("warn") == Level::Warning);
    CHECK(parse_level("error") == Level::Error);
    CHECK(parse_level("err") == Level::Error);
    CHECK(parse_level("critical") == Level::Critical);
    CHECK(parse_level("fatal") == Level::Critical);
    CHECK(parse_level("none") == Level::None);
    CHECK(parse_level("off") == Level::None);
    CHECK(!parse_level(""));
    CHECK(!parse_level("verbose"));
    CHECK(!parse_level("this-string-is-far-too-long-to-be-a-level"));
    for (Level l : {Level::Trace, Level::Debug, Level::Info, Level::Warning, Level::Error,
                    Level::Critical, Level::None})
        CHECK(parse_level(c_log::level_name(l)) == l);  // round-trips

    // set_level_from_env: unset variable leaves the level alone.
    {
        c_log::Logger log(test::quiet_options());
        CHECK(!log.set_level_from_env("CLOGPP_TEST_SURELY_UNSET_VARIABLE"));
        CHECK(log.level() == Level::Trace);
    }
    return test::result();
}
