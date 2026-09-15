// One logger, several destinations with different levels and formats:
//   - JSON, everything, to a size-rotated file
//   - JSON, everything, to a daily file
//   - human-readable text, warnings and above, to the terminal
#include <clogpp/clogpp.hpp>

int main() {
    c_log::Logger::Options o;
    o.level = c_log::Level::Debug;
    o.console = false;  // we add our own console sink below
    c_log::Logger log(o);

    // Rotate at 1 MB, keep 3 backups: app.log -> app.log.1 -> ... -> app.log.3
    log.add_sink<c_log::RotatingFileSink>("app.log", 1024 * 1024, 3);

    // One file per day: logs/app_YYYY-MM-DD.log, keep the newest 7.
    log.add_sink<c_log::DailyFileSink>("logs/app.log", 7);

    // Terminal: text format, coloured when stderr is a TTY, warnings only.
    auto console = std::make_unique<c_log::ConsoleSink>(stderr);
    console->set_level(c_log::Level::Warning);
    console->set_formatter<c_log::TextFormatter>(
        c_log::TextFormatter::stream_supports_color(stderr));
    log.add_sink(std::move(console));

    log.debug("config.loaded").kv("entries", 12);  // files only
    log.info("server.listen").kv("port", 8080);    // files only
    log.warn("disk.low").kv("free_mb", 512);       // files + terminal
    log.error("db.timeout").kv("ms", 3000).kv("retry", true);

    // Keep the last 100 lines in memory too, e.g. to attach to a crash report.
    log.add_sink<c_log::MemorySink>(100);
    log.critical("example.done");
}
