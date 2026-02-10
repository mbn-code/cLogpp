#include "../include/logger.hpp"
#include "../include/logger_config.hpp"
#include <memory>

int main() {
    // Load config from file if exists, fallback to default async logger
    auto log = c_log::logger_from_config("logger.json");
    log->info("app.start").kv("version", "0.1");
    log->error("db.fail").kv("query", "SELECT * FROM foo").kv("code", 10);
    log->set_level(c_log::Level::Warning);
    log->warn("low.battery").kv("percent", 15);
    // manual flush happens automatically at destruction
    return 0;
}
