// cLog++ micro-benchmark.
//
// Each scenario times N end-to-end log calls INCLUDING the logger's
// destructor, which fully drains the async queue before returning. Async mode
// is lossless by default (it applies backpressure when the queue is full), so
// the numbers reflect real delivery, not dropped entries.
//
// Scenarios (sink x mode):
//   null      - a discard sink, to isolate serialise + queue overhead
//   file      - real FileSink I/O (the console sink is removed)
//   text      - discard sink with the TextFormatter instead of JSON
//   context   - discard sink; every entry also carries 2 bound + 2 scoped fields
//   4-threads - four producer threads sharing one async logger (N calls total)
//
// Results are written to benchmark_results.csv (read by plot_benchmarks.py)
// and echoed to stdout. Run with --quick for a shorter N.

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include <clogpp/clogpp.hpp>

using namespace std::chrono;
using c_log::Logger;

static int N = 200000;

template <typename Fn>
static void run_bench(std::ostream& csv, const char* sink, const char* mode, Fn fn) {
    // Warm-up run (allocator, page cache), then the timed run.
    fn();
    const auto start = steady_clock::now();
    fn();
    const auto end = steady_clock::now();
    const double usec = duration<double, std::micro>(end - start).count() / N;
    csv << "cLog++," << sink << "," << mode << "," << usec << "\n";
    std::cout << "cLog++," << sink << "," << mode << "," << usec << "\n";
}

static Logger::Options quiet(Logger::Mode mode) {
    Logger::Options o;
    o.mode = mode;
    o.console = false;
    return o;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--quick") == 0) N = 20000;

    const char* csv_path = "benchmark_results.csv";
    std::ofstream csv(csv_path);
    csv << "logger,sink,mode,usec_per_log\n";
    std::cout << "logger,sink,mode,usec_per_log\n";

    const char* bench_file = "bench_cLogpp_file.txt";

    for (Logger::Mode mode : {Logger::Mode::Sync, Logger::Mode::Async}) {
        const char* m = mode == Logger::Mode::Sync ? "sync" : "async";

        run_bench(csv, "null", m, [&] {
            Logger log(quiet(mode));
            log.add_sink<c_log::NullSink>();
            for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
        });

        run_bench(csv, "file", m, [&] {
            Logger log(quiet(mode));
            log.add_sink<c_log::FileSink>(bench_file, /*truncate=*/true);
            for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
        });

        run_bench(csv, "text", m, [&] {
            Logger log(quiet(mode));
            log.set_formatter<c_log::TextFormatter>();
            log.add_sink<c_log::NullSink>();
            for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
        });

        run_bench(csv, "context", m, [&] {
            Logger log(quiet(mode));
            log.add_sink<c_log::NullSink>();
            log.bind("service", "bench").bind("version", 3);
            c_log::ScopedContext ctx;
            ctx.kv("request_id", "r-123").kv("user", 42);
            for (int i = 0; i < N; ++i) log.info("bench").kv("i", i);
        });
    }

    run_bench(csv, "null", "async-4-threads", [&] {
        Logger log(quiet(Logger::Mode::Async));
        log.add_sink<c_log::NullSink>();
        const int threads = 4;
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&] {
                for (int i = 0; i < N / threads; ++i) log.info("bench").kv("i", i);
            });
        for (auto& th : pool) th.join();
    });

    // Filtered-out entries: what a disabled debug() costs on the hot path.
    run_bench(csv, "null", "filtered", [&] {
        Logger log(quiet(Logger::Mode::Sync));
        log.add_sink<c_log::NullSink>();
        log.set_level(c_log::Level::Warning);
        for (int i = 0; i < N; ++i) log.debug("bench").kv("i", i);
    });

    std::remove(bench_file);
    std::cout << "\nWrote " << csv_path << " (N=" << N << ")\n";
    return 0;
}
