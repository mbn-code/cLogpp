// RotatingFileSink rotates once the active file would exceed max_bytes, keeping
// a bounded number of backups; max_bytes == 0 disables rotation.
#include <cstdio>
#include <fstream>

#include "test_util.hpp"

static bool exists(const std::string& p) {
    std::ifstream f(p);
    return f.good();
}
static std::streamoff file_size(const std::string& p) {
    std::ifstream in(p, std::ios::ate | std::ios::binary);
    return in.tellg();
}
static void cleanup(const std::string& base) {
    for (const std::string& p : {base, base + ".1", base + ".2", base + ".3", base + ".4"})
        std::remove(p.c_str());
}

int main() {
    const std::string base = "rotate_test.log";
    cleanup(base);

    {
        c_log::Logger log(test::quiet_options());
        auto sink = std::make_unique<c_log::RotatingFileSink>(base, /*max_bytes=*/120,
                                                              /*max_files=*/2);
        auto* raw = sink.get();
        log.add_sink(std::move(sink));
        for (int i = 0; i < 50; ++i) log.info("rotate.me").kv("seq", i).kv("pad", "xxxxxxxxxx");
        CHECK(raw->rotations() > 2);
        CHECK(raw->current_size() <= 240);
    }

    // Base file exists, plus exactly the 2 allowed backups; never a 3rd.
    CHECK(exists(base));
    CHECK(exists(base + ".1"));
    CHECK(exists(base + ".2"));
    CHECK(!exists(base + ".3"));

    // The active file must respect the size bound (within one line of slack),
    // and the newest backup holds the most recent rotated content.
    const std::streamoff sz = file_size(base);
    CHECK(sz > 0 && sz <= 240);
    CHECK(file_size(base + ".1") <= 240);
    {
        std::ifstream in(base + ".1");
        std::string first;
        std::getline(in, first);
        CHECK_CONTAINS(first, "rotate.me");
    }
    cleanup(base);

    // max_bytes == 0: never rotates, just appends.
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::RotatingFileSink>(base, 0, 2);
        for (int i = 0; i < 50; ++i) log.info("no.rotate").kv("seq", i);
    }
    CHECK(exists(base));
    CHECK(!exists(base + ".1"));
    CHECK(file_size(base) > 240);
    cleanup(base);

    // max_files == 0: rotation truncates in place, no backups are created.
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::RotatingFileSink>(base, 120, 0);
        for (int i = 0; i < 50; ++i) log.info("truncate").kv("seq", i).kv("pad", "xxxxxxxxxx");
    }
    CHECK(exists(base));
    CHECK(!exists(base + ".1"));
    CHECK(file_size(base) <= 240);
    cleanup(base);
    return test::result();
}
