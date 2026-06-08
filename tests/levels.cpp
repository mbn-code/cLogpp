// Regression test: set_level() must actually filter out lower-severity entries.
#include "logger.hpp"
#include <cassert>
#include <string>
#include <vector>

// Captures into an external vector so the lines outlive the Logger (which owns
// and destroys its sinks).
struct CaptureSink : c_log::Sink {
    std::vector<std::string>* lines;
    explicit CaptureSink(std::vector<std::string>* v) : lines(v) {}
    void log(const std::string& msg) override { lines->push_back(msg); }
};

int main() {
    std::vector<std::string> lines;
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink(std::make_unique<CaptureSink>(&lines));

        log.set_level(c_log::Level::Warning);
        log.trace("t");           // filtered
        log.debug("d");           // filtered
        log.info("i").kv("x", 1); // filtered
        log.warn("w");            // kept
        log.error("e");           // kept
        log.critical("c");        // kept
    }

    assert(lines.size() == 3); // only warn, error, critical survive
    assert(lines[0].find("\"event\":\"w\"") != std::string::npos);
    assert(lines[1].find("\"event\":\"e\"") != std::string::npos);
    assert(lines[2].find("\"event\":\"c\"") != std::string::npos);

    // Level::None silences everything.
    std::vector<std::string> none_lines;
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink(std::make_unique<CaptureSink>(&none_lines));
        log.set_level(c_log::Level::None);
        log.critical("should.not.appear");
    }
    assert(none_lines.empty());
    return 0;
}
