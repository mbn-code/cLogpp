#include "../include/logger.hpp"
#include <cassert>
int main() {
    c_log::Logger log;
    log.info("test.event").kv("a", 123);
    // No assertion crash == pass
    return 0;
}

