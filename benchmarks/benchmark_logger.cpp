#include "logger.hpp"
#include "file_sink.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace std::chrono;

constexpr int N = 100000;

template <typename Fn>
double run_bench(const char* label, Fn fn) {
    auto start = high_resolution_clock::now();
    fn();
    auto end = high_resolution_clock::now();
    double ms = duration<double, std::micro>(end - start).count() / N;
    std::cout << label << ", " << ms << "\n";
    return ms;
}

int main() {
    std::ofstream file("bench_cLogpp_file.txt");
    std::cout << "logger,sink,mode,usec_per_log\n";

    // async file
    run_bench("cLog++,file,async", [&]() {
        c_log::Logger log(c_log::Logger::Mode::Async);
        log.add_sink(std::make_unique<c_log::FileSink>("./bench_cLogpp_file.txt"));
        for (int i = 0; i < N; ++i)
            log.info("bench").kv("i", i);
    });

    // sync file
    run_bench("cLog++,file,sync", [&]() {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.add_sink(std::make_unique<c_log::FileSink>("./bench_cLogpp_file.txt"));
        for (int i = 0; i < N; ++i)
            log.info("bench").kv("i", i);
    });

    // async console
    run_bench("cLog++,console,async", [&]() {
        c_log::Logger log(c_log::Logger::Mode::Async);
        for (int i = 0; i < N; ++i)
            log.info("bench").kv("i", i);
    });

    // sync console
    run_bench("cLog++,console,sync", [&]() {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        for (int i = 0; i < N; ++i)
            log.info("bench").kv("i", i);
    });

    // no-op baseline
    run_bench("noop,none,none", [&]() {
        for (int i = 0; i < N; ++i) {
            int x = i * 2 + 1;
            volatile int sink = x;
        }
    });

    return 0;
}
