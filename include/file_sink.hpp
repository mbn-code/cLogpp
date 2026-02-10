#pragma once
#include <string>
#include <fstream>
#include "logger.hpp"

namespace c_log {

class FileSink : public Sink {
public:
    explicit FileSink(const std::string& filename) : file_(filename, std::ios::app) {}
    void log(const std::string& msg) override {
        if (file_) file_ << msg << '\n';
    }
private:
    std::ofstream file_;
};

} // namespace c_log
