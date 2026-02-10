#include "../include/logger.hpp"

int main() {
    c_log::Logger log;
    log.info("system.start")
       .kv("version", "1.0")
       .kv("user_count", 5);
    // flush current record on destruction
    return 0;
}
