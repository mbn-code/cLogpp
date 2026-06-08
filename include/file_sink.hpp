#pragma once
#include <string>
#include <fstream>
#include "logger.hpp"

namespace c_log {

// Appends each log line to a file. Open with append (default) or truncate.
class FileSink : public Sink {
public:
    explicit FileSink(const std::string& filename, bool truncate = false)
        : file_(filename, truncate ? (std::ios::out | std::ios::trunc)
                                    : (std::ios::out | std::ios::app)) {}
    void log(const std::string& msg) override {
        if (file_) file_ << msg << '\n';
    }
    void flush() override { file_.flush(); }
    bool is_open() const { return file_.is_open(); }
private:
    std::ofstream file_;
};

} // namespace c_log
