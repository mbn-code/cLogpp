#include "../include/logger.hpp"

int main() {
    Logger log; // async by default
    log.info("app.start").kv("version", "0.1").send();
    log.error("db.fail").kv("query", "SELECT * FROM foo").kv("code", 10).send();
    log.set_level(Logger::Level::Warning);
    log.warn("low.battery").kv("percent", 15).send();
}
