// cLog++ micro-benchmark.
//
// Each scenario times N end-to-end log calls, INCLUDING the logger's
// destructor, which fully drains the async queue before returning. Async mode
// is lossless (it applies backpressure when the queue is full), so the file
// numbers reflect real delivery, not dropped entries.
//
// Scenarios:
//   file  - real FileSink I/O (the console sink is removed so we measure file only)
//   null  - a discard sink, to isolate the logger's serialize + queue overhead
//
// Results are written to benchmark_results.csv (read by plot_benchmarks.py) and
// echoed to stdout.

#include "logger.hpp"
#include "file_sink.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace std::chrono;

constexpr int N = 100000;

// Sink that discards everything, so the benchmark measures the logger itself.
struct NullSink : c_log::Sink {
    void log(const std::string&) override {}
};

template <typename Fn>
double run_bench(std::ostream& csv, const char* logger, const char* sink, const char* mode, Fn fn) {
    auto start = high_resolution_clock::now();
    fn();
    auto end = high_resolution_clock::now();
    double usec = duration<double, std::micro>(end - start).count() / N;
    csv << logger << "," << sink << "," << mode << "," << usec << "\n";
    std::cout << logger << "," << sink << "," << mode << "," << usec << "\n";
    return usec;
}

int main() {
    const char* csv_path = "benchmark_results.csv";
    std::ofstream csv(csv_path);
    csv << "logger,sink,mode,usec_per_log\n";
    std::cout << "logger,sink,mode,usec_per_log\n";

    const char* bench_file = "bench_cLogpp_file.txt";

    run_bench(csv, "cLog++", "file", "async", [&] {
        c_log::Logger log(c_log::Logger::Mode::Async);
        log.clear_sinks();
        log.add_sink(std::make_unique<c_log::FileSink>(bench_file, /*truncate=*/true));
        for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
    });

    run_bench(csv, "cLog++", "file", "sync", [&] {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink(std::make_unique<c_log::FileSink>(bench_file, /*truncate=*/true));
        for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
    });

    run_bench(csv, "cLog++", "null", "async", [&] {
        c_log::Logger log(c_log::Logger::Mode::Async);
        log.clear_sinks();
        log.add_sink(std::make_unique<NullSink>());
        for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
    });

    run_bench(csv, "cLog++", "null", "sync", [&] {
        c_log::Logger log(c_log::Logger::Mode::Sync);
        log.clear_sinks();
        log.add_sink(std::make_unique<NullSink>());
        for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
    });

    run_bench(csv, "noop", "none", "none", [&] {
        volatile int sink = 0;
        for (int i = 0; i < N; ++i) sink = i * 2 + 1;
        (void)sink;
    });

    std::cout << "\nWrote " << csv_path << "\n";
    return 0;
}
