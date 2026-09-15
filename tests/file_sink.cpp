// FileSink: append vs truncate, flush on destruction, is_open() on bad paths.
#include <cstdio>
#include <fstream>

#include "test_util.hpp"

static std::vector<std::string> read_lines(const char* path) {
    std::vector<std::string> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) out.push_back(line);
    return out;
}

int main() {
    const char* fname = "test_file_sink.log";
    std::remove(fname);
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        auto sink = std::make_unique<c_log::FileSink>(fname);
        CHECK(sink->is_open());
        CHECK(sink->path() == fname);
        log.add_sink(std::move(sink));
        log.info("write_to_file").kv("val", 42);
    }  // Logger destroyed, sinks flushed
    auto lines = read_lines(fname);
    CHECK(lines.size() == 1);
    CHECK_CONTAINS(lines[0], "\"event\":\"write_to_file\"");
    CHECK_CONTAINS(lines[0], "\"val\":42");

    // Append mode keeps the previous content.
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink<c_log::FileSink>(fname);
        log.info("appended");
    }
    lines = read_lines(fname);
    CHECK(lines.size() == 2);
    CHECK_CONTAINS(lines[1], "appended");

    // Truncate mode starts over.
    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink<c_log::FileSink>(fname, /*truncate=*/true);
        log.info("fresh");
    }
    lines = read_lines(fname);
    CHECK(lines.size() == 1);
    CHECK_CONTAINS(lines[0], "fresh");

    // An unwritable path yields a closed sink; logging to it does not throw.
    {
        c_log::FileSink bad("no_such_dir/definitely/missing.log");
        CHECK(!bad.is_open());
        bad.log("dropped");
        bad.flush();
    }
    std::remove(fname);
    return test::result();
}
