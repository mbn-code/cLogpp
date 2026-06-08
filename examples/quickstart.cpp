#include "../include/logger.hpp"
#include <memory>

int main() {
    // Default async logger; the built-in console sink writes to stderr.
    c_log::Logger log;
    log.info("app.start").kv("version", "0.1");
    
    // Demonstrate filtering
    log.set_level(c_log::Level::Warning);
    
    // This info log will be skipped
    log.info("app.loop");
    
    log.error("db.fail").kv("query", "SELECT * FROM foo").kv("code", 10);
    log.warn("low.battery").kv("percent", 15);

    // Logger flushes automatically at destruction
    return 0;
}
