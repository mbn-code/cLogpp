// Sync mode: entries are emitted immediately, in order, to every sink.
#include "logger.hpp"
#include <cassert>
#include <string>
#include <vector>

struct CaptureSink : c_log::Sink {
    std::vector<std::string> lines;
    void log(const std::string& msg) override { lines.push_back(msg); }
};

int main() {
    CaptureSink* cap = new CaptureSink;
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks(); // drop the default console sink
        log.add_sink(std::unique_ptr<c_log::Sink>(cap));
        log.info("sync_test").kv("x", 17);
        // In sync mode the line is already in cap->lines here.
        assert(cap->lines.size() == 1);
        assert(cap->lines[0].find("\"event\":\"sync_test\"") != std::string::npos);
        assert(cap->lines[0].find("\"x\":17") != std::string::npos); // native number, not "17"
        assert(cap->lines[0].find("\"level\":\"info\"") != std::string::npos);
    }
    return 0;
}
