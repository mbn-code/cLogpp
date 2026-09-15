// Structured context: logger-wide bound fields, per-thread scoped context,
// source locations and thread ids.
#include <clogpp/clogpp.hpp>

#include <thread>

static void handle_request(c_log::Logger& log, int request_id) {
    // Everything logged on this thread while `ctx` lives carries request_id.
    c_log::ScopedContext ctx;
    ctx.kv("request_id", request_id);

    log.info("request.start");
    {
        c_log::ScopedContext span;  // nested: adds to the outer context
        span.kv("span", "db.query");
        log.debug("query.run").kv("rows", 12);
    }
    log.info("request.done").kv("status", 200);
}

int main() {
    c_log::Logger::Options o;
    o.level = c_log::Level::Debug;
    o.source_location = true;  // adds file/line/func to every entry
    o.thread_id = true;        // adds tid
    c_log::Logger log(o);

    // Bound fields: attached to every entry from this logger.
    log.bind("service", "checkout").bind("version", 3);

    std::thread a(handle_request, std::ref(log), 1);
    std::thread b(handle_request, std::ref(log), 2);
    a.join();
    b.join();

    log.info("main.done");
}
