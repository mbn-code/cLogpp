// Regression test: every line carries a timestamp + level + event, and numeric
// and boolean fields are emitted as native JSON values (not quoted strings).
#include "logger.hpp"
#include <cassert>
#include <string>
#include <vector>

// Captures into an external vector so the line outlives the Logger.
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
        log.error("db.fail")
           .kv("code", 503)
           .kv("ok", false)
           .kv("ratio", 0.5)
           .kv("note", "a\"b");
    }
    assert(lines.size() == 1);
    const std::string& l = lines[0];

    assert(l.find("\"level\":\"error\"") != std::string::npos);
    assert(l.find("\"event\":\"db.fail\"") != std::string::npos);
    assert(l.find("\"ts\":\"") != std::string::npos);
    assert(l.find("Z\"") != std::string::npos);          // ISO-8601 UTC suffix
    assert(l.find("\"code\":503") != std::string::npos); // number, unquoted
    assert(l.find("\"code\":\"503\"") == std::string::npos);
    assert(l.find("\"ok\":false") != std::string::npos);  // bool, unquoted
    assert(l.find("\"ratio\":0.5") != std::string::npos); // double, unquoted
    assert(l.find("\"note\":\"a\\\"b\"") != std::string::npos); // string escaped
    return 0;
}
