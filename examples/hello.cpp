// The smallest possible program: async logger, JSON to stderr.
#include <clogpp/clogpp.hpp>

int main() {
    c_log::Logger log;
    log.info("system.start").kv("version", c_log::version()).kv("user_count", 5);
    // The logger flushes and joins its worker when it goes out of scope.
}
