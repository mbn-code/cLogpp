// Development-friendly output: text format with colours on the terminal,
// microsecond timestamps, and the level taken from CLOG_LEVEL if set.
#include <clogpp/clogpp.hpp>

int main() {
    c_log::Logger log(c_log::Logger::Mode::Sync);
    log.set_level(c_log::Level::Trace);
    log.set_level_from_env();  // CLOG_LEVEL=warn ./example_text_console

    c_log::TextFormatter::Options fmt;
    fmt.color = c_log::TextFormatter::stream_supports_color(stderr);
    fmt.precision = c_log::TimePrecision::Microseconds;
    log.set_formatter<c_log::TextFormatter>(fmt);

    log.trace("boot.probe").kv("cpu", 8);
    log.debug("config.read").kv("path", "/etc/app.toml");
    log.info("server.start").kv("port", 8080).kv("tls", true);
    log.warn("disk.low").kv("free_mb", 512);
    log.error("db.connect").kv("host", "db-1").kv("attempt", 3);
    log.critical("giving.up").message("no database after 3 attempts");
}
