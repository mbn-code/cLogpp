// RotatingFileSink rotates once the active file would exceed max_bytes, keeping
// a bounded number of backups.
#include "logger.hpp"
#include "rotating_file_sink.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

static bool exists(const std::string& p) {
    std::ifstream f(p);
    return f.good();
}

int main() {
    const std::string base = "rotate_test.log";
    // Clean any leftovers from a previous run.
    for (const std::string& p : {base, base + ".1", base + ".2", base + ".3", base + ".4"})
        std::remove(p.c_str());

    {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        // ~120 bytes per rotation, keep 2 backups.
        log.add_sink(std::make_unique<c_log::RotatingFileSink>(base, /*max_bytes=*/120, /*max_files=*/2));
        for (int i = 0; i < 50; ++i)
            log.info("rotate.me").kv("seq", i).kv("pad", "xxxxxxxxxx");
    }

    // Base file exists, plus exactly the 2 allowed backups; never a 3rd.
    assert(exists(base));
    assert(exists(base + ".1"));
    assert(exists(base + ".2"));
    assert(!exists(base + ".3"));

    // The active file must respect the size bound (within one line of slack).
    std::ifstream in(base, std::ios::ate | std::ios::binary);
    std::streamoff sz = in.tellg();
    assert(sz > 0 && sz <= 240); // bounded; one over-limit line of slack tolerated

    for (const std::string& p : {base, base + ".1", base + ".2"})
        std::remove(p.c_str());
    return 0;
}
