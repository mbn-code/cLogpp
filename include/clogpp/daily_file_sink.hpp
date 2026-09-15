#pragma once
// Time-based rotating file sink: one file per calendar day.

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <string>
#include <utility>
#include <vector>

#include "clogpp/sinks.hpp"

namespace c_log {

// Writes to `<stem>_YYYY-MM-DD<ext>` next to base_path and switches to a new
// file at local midnight, e.g. "logs/app.log" -> "logs/app_2026-09-15.log".
//
// With max_files > 0 the sink keeps only the newest max_files dated files for
// this base path (older ones, including those from earlier runs, are removed
// after each rotation). Missing parent directories are created.
class DailyFileSink : public Sink {
public:
    // The clock is injectable for tests; it must return seconds since the
    // Unix epoch (like std::time(nullptr)).
    using Clock = std::function<std::time_t()>;

    explicit DailyFileSink(std::string base_path, int max_files = 0, Clock clock = {})
        : max_files_(max_files < 0 ? 0 : max_files),
          clock_(clock ? std::move(clock) : Clock([] { return std::time(nullptr); })) {
        std::filesystem::path p(base_path);
        dir_ = p.parent_path();
        stem_ = p.stem().string();
        ext_ = p.extension().string();
        reopen(clock_());
    }

    void log(const std::string& line) override {
        const std::time_t now = clock_();
        if (now >= next_rotation_) reopen(now);
        if (file_) file_ << line << '\n';
    }

    void flush() override { file_.flush(); }
    bool is_open() const { return file_.is_open(); }
    // Path of the file currently being written.
    const std::string& current_path() const noexcept { return current_path_; }
    // Number of times the sink switched files (excluding the initial open).
    std::size_t rotations() const noexcept { return rotations_; }

private:
    static std::tm local_time(std::time_t t) {
        std::tm tmv{};
#if defined(_WIN32)
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        return tmv;
    }

    void reopen(std::time_t now) {
        if (file_.is_open()) {
            file_.close();
            ++rotations_;
        }
        std::tm tmv = local_time(now);
        char date[32];
        std::snprintf(date, sizeof(date), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
                      tmv.tm_mday);

        std::error_code ec;
        if (!dir_.empty()) std::filesystem::create_directories(dir_, ec);
        current_path_ = (dir_ / (stem_ + "_" + date + ext_)).string();
        file_.clear();
        file_.open(current_path_, std::ios::out | std::ios::app);

        // Next local midnight. mktime normalises the day overflow and picks the
        // right DST offset because tm_isdst is -1.
        tmv.tm_mday += 1;
        tmv.tm_hour = 0;
        tmv.tm_min = 0;
        tmv.tm_sec = 0;
        tmv.tm_isdst = -1;
        next_rotation_ = std::mktime(&tmv);
        if (next_rotation_ == static_cast<std::time_t>(-1)) next_rotation_ = now + 86400;

        if (max_files_ > 0) prune();
    }

    // Keep the newest max_files_ files named <stem>_YYYY-MM-DD<ext>.
    void prune() {
        std::error_code ec;
        const std::filesystem::path dir = dir_.empty() ? std::filesystem::path(".") : dir_;
        std::vector<std::filesystem::path> dated;
        const std::string prefix = stem_ + "_";
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            const std::string name = entry.path().filename().string();
            if (name.size() != prefix.size() + 10 + ext_.size()) continue;
            if (name.compare(0, prefix.size(), prefix) != 0) continue;
            if (name.compare(name.size() - ext_.size(), ext_.size(), ext_) != 0) continue;
            if (!is_date(name.substr(prefix.size(), 10))) continue;
            dated.push_back(entry.path());
        }
        if (dated.size() <= static_cast<std::size_t>(max_files_)) return;
        std::sort(dated.begin(), dated.end());  // YYYY-MM-DD sorts chronologically
        for (std::size_t i = 0; i + static_cast<std::size_t>(max_files_) < dated.size(); ++i)
            std::filesystem::remove(dated[i], ec);
    }

    static bool is_date(const std::string& s) {
        if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (i == 4 || i == 7) continue;
            if (s[i] < '0' || s[i] > '9') return false;
        }
        return true;
    }

    std::filesystem::path dir_;
    std::string stem_;
    std::string ext_;
    int max_files_;
    Clock clock_;
    std::ofstream file_;
    std::string current_path_;
    std::time_t next_rotation_ = 0;
    std::size_t rotations_ = 0;
};

}  // namespace c_log
