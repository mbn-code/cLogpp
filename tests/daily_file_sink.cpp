// DailyFileSink: dated file names, rotation at local midnight (driven by an
// injected clock), directory creation, and pruning of old files.
#include <ctime>
#include <filesystem>
#include <fstream>

#include "test_util.hpp"

namespace fs = std::filesystem;

static std::string date_of(std::time_t t) {
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
                  tmv.tm_mday);
    return buf;
}

static std::size_t count_lines(const fs::path& p) {
    std::ifstream in(p);
    std::string line;
    std::size_t n = 0;
    while (std::getline(in, line)) ++n;
    return n;
}

int main() {
    const fs::path dir = "daily_test_dir";
    std::error_code ec;
    fs::remove_all(dir, ec);

    // Start at 2026-06-10 23:59:00 local time (no DST transition nearby in any
    // common zone) and advance across midnight.
    std::tm start{};
    start.tm_year = 2026 - 1900;
    start.tm_mon = 5;
    start.tm_mday = 10;
    start.tm_hour = 23;
    start.tm_min = 59;
    start.tm_isdst = -1;
    std::time_t now = std::mktime(&start);
    CHECK(now != static_cast<std::time_t>(-1));

    const std::string base = (dir / "app.log").string();
    {
        c_log::Logger log(test::quiet_options());
        auto sink =
            std::make_unique<c_log::DailyFileSink>(base, /*max_files=*/2, [&now] { return now; });
        c_log::DailyFileSink* d = sink.get();
        CHECK(d->is_open());  // parent directory was created
        CHECK(fs::is_directory(dir));
        CHECK(d->current_path() == (dir / ("app_" + date_of(now) + ".log")).string());
        log.add_sink(std::move(sink));

        log.info("day1.a");
        now += 30;  // still before midnight
        log.info("day1.b");
        CHECK(d->rotations() == 0);

        now += 120;  // 00:01 next day
        log.info("day2.a");
        CHECK(d->rotations() == 1);
        CHECK(d->current_path() == (dir / ("app_" + date_of(now) + ".log")).string());

        now += 86400;  // day 3
        log.info("day3.a");
        CHECK(d->rotations() == 2);

        now += 86400;  // June 13: with max_files=2, June 10 and 11 are pruned
        log.info("day4.a");
        CHECK(d->rotations() == 3);
    }

    std::vector<std::string> names;
    for (const auto& e : fs::directory_iterator(dir)) names.push_back(e.path().filename().string());
    std::sort(names.begin(), names.end());
    CHECK(names.size() == 2);
    CHECK(names.size() == 2 && names[0] == "app_2026-06-12.log");
    CHECK(names.size() == 2 && names[1] == "app_2026-06-13.log");
    CHECK(count_lines(dir / "app_2026-06-12.log") == 1);
    CHECK(count_lines(dir / "app_2026-06-13.log") == 1);

    // A new sink on the same day appends to that day's file and prunes stale
    // dated files from earlier runs, but never unrelated files with a similar prefix.
    {
        std::ofstream(dir / "app_notes.log") << "keep\n";
        std::ofstream(dir / "app_2026-06-01.txt") << "keep\n";
        std::ofstream(dir / "app_2026-06-02.log") << "old dated file from a previous run\n";
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::DailyFileSink>(base, 2, [&now] { return now; });
        log.info("day4.b");
    }
    CHECK(fs::exists(dir / "app_notes.log"));
    CHECK(fs::exists(dir / "app_2026-06-01.txt"));
    CHECK(!fs::exists(dir / "app_2026-06-02.log"));       // dated, oldest: pruned
    CHECK(fs::exists(dir / "app_2026-06-12.log"));        // still among the newest two
    CHECK(count_lines(dir / "app_2026-06-13.log") == 2);  // appended, not truncated

    // No pruning with max_files == 0; base path without a directory works.
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<c_log::DailyFileSink>("daily_flat.log", 0, [&now] { return now; });
        log.info("flat");
    }
    const fs::path flat = "daily_flat_" + date_of(now) + ".log";
    CHECK(fs::exists(flat));
    fs::remove(flat, ec);
    fs::remove_all(dir, ec);
    return test::result();
}
