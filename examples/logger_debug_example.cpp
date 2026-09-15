// Trace/debug output plus the tiny debug_log() helper (compiled out with
// -DCLOG_DISABLE_DEBUG).
#include <clogpp/clogpp.hpp>

#include "logger_debug.hpp"

int main() {
    c_log::Logger log(c_log::Logger::Mode::Sync);
    log.set_level(c_log::Level::Trace);  // show trace/debug (default minimum is Info)
    log.debug("debug.tst").kv("what", "testing");
    c_log::debug_log("This only prints in debug mode");
    log.trace("a.trace").kv("data", 1);
    log.critical("a.critical").kv("msg", "shutdown");
}
