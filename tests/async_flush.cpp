#include "../include/logger.hpp"
#include "../include/file_sink.hpp"
#include <fstream>
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>

int main() {
    const char* fname = "test_async_flush.log";
    std::remove(fname);
    {
        c_log::Logger log; // default async
        log.add_sink(std::make_unique<c_log::FileSink>(fname));
        log.info("asyncflush").kv("payload", 12345);
        // No delay: logger goes out of scope instantly
    }

    std::ifstream in(fname);
    std::string line;
    std::getline(in, line);
    assert(line.find("asyncflush") != std::string::npos);
    assert(line.find("12345") != std::string::npos);
    return 0;
}
