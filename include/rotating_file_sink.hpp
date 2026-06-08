#pragma once
#include <string>
#include <fstream>
#include <ios>
#include <cstdio>
#include "logger.hpp"

namespace c_log {

// Size-based rotating file sink.
//
// Lines are appended to base_path. When writing the next line would push the
// file past max_bytes, the sink rotates: base_path -> base_path.1 ->
// base_path.2 -> ... up to max_files backups; the oldest is discarded. Set
// max_bytes to 0 to disable rotation (behaves like a plain append sink).
class RotatingFileSink : public Sink {
public:
    RotatingFileSink(std::string base_path, std::size_t max_bytes, int max_files = 3)
        : base_path_(std::move(base_path)),
          max_bytes_(max_bytes),
          max_files_(max_files < 0 ? 0 : max_files) {
        open(/*truncate=*/false);
    }

    void log(const std::string& msg) override {
        const std::size_t need = msg.size() + 1; // + newline
        if (max_bytes_ != 0 && current_size_ > 0 && current_size_ + need > max_bytes_)
            rotate();
        file_ << msg << '\n';
        current_size_ += need;
    }

    void flush() override { file_.flush(); }
    bool is_open() const { return file_.is_open(); }

private:
    void open(bool truncate) {
        file_.clear();
        file_.open(base_path_, truncate ? (std::ios::out | std::ios::trunc)
                                        : (std::ios::out | std::ios::app));
        std::streamoff off = 0;
        if (file_) off = static_cast<std::streamoff>(file_.tellp());
        current_size_ = off > 0 ? static_cast<std::size_t>(off) : 0;
    }

    void rotate() {
        file_.close();
        if (max_files_ == 0) {        // no backups: start the base file over
            open(/*truncate=*/true);
            return;
        }
        std::remove((base_path_ + "." + std::to_string(max_files_)).c_str());
        for (int i = max_files_ - 1; i >= 1; --i) {
            std::string src = base_path_ + "." + std::to_string(i);
            std::string dst = base_path_ + "." + std::to_string(i + 1);
            std::rename(src.c_str(), dst.c_str()); // ignore failure: src may not exist
        }
        std::rename(base_path_.c_str(), (base_path_ + ".1").c_str());
        open(/*truncate=*/true);      // fresh, empty base file
    }

    std::string base_path_;
    std::size_t max_bytes_;
    int max_files_;
    std::ofstream file_;
    std::size_t current_size_ = 0;
};

} // namespace c_log
