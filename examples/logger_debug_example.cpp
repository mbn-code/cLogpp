#include "logger.hpp"
#include "logger_debug.hpp"

int main() {
    c_log::Logger log(c_log::Logger::Mode::Sync);
    log.debug("debug.tst").kv("what", "testing");
    c_log::debug_log("This only prints in debug mode");
    log.trace("a.trace").kv("data", 1);
    log.critical("a.critical").kv("msg", "shutdown");
    return 0;
}
