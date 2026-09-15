#pragma once
// Plain append-to-file sink.

#include <fstream>
#include <ios>
#include <string>

#include "clogpp/sinks.hpp"

namespace c_log {

// Appends each log line to a file. Open with append (default) or truncate.
// Check is_open() after construction if the path might be unwritable; a
// closed sink silently drops lines rather than throwing from the log path.
class FileSink : public Sink {
public:
    explicit FileSink(const std::string& filename, bool truncate = false)
        : path_(filename),
          file_(filename,
                truncate ? (std::ios::out | std::ios::trunc) : (std::ios::out | std::ios::app)) {}

    void log(const std::string& line) override {
        if (file_) file_ << line << '\n';
    }
    void flush() override { file_.flush(); }

    bool is_open() const { return file_.is_open(); }
    const std::string& path() const noexcept { return path_; }

private:
    std::string path_;
    std::ofstream file_;
};

}  // namespace c_log
