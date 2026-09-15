// A tour of the everyday API: levels, typed fields, filtering, flush.
#include <clogpp/clogpp.hpp>

int main() {
    // Default async logger; the built-in console sink writes JSON to stderr.
    c_log::Logger log;
    log.info("app.start").kv("version", "0.3").kv("pid", 4242);

    // Fields keep their types: strings are quoted, numbers and bools are not.
    log.info("user.login").kv("id", 42).kv("admin", false).kv("ratio", 0.75);

    // Free-form text goes in "msg"; the event name stays a stable identifier.
    log.warn("cache.miss").message("falling back to origin").kv("key", "user:42");

    // Filtering: everything below the level is skipped before any work is done.
    log.set_level(c_log::Level::Warning);
    log.info("app.loop");  // skipped
    log.error("db.fail").kv("query", "SELECT * FROM foo").kv("code", 10);

    // Read the level from the environment (CLOG_LEVEL=debug ./example_quickstart).
    if (log.set_level_from_env()) log.warn("level.from_env");

    // Block until everything so far is written, e.g. before a risky operation.
    log.flush();
    log.critical("shutdown").kv("reason", "example finished");
    // Logger flushes automatically at destruction.
}
