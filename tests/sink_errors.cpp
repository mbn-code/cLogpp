// A throwing sink must not kill the async worker or poison other sinks; the
// failure is counted and reported through the error handler.
#include <stdexcept>

#include "test_util.hpp"

struct ThrowingSink : c_log::Sink {
    int calls = 0;
    void log(const std::string&) override {
        ++calls;
        if (calls % 2 == 0) throw std::runtime_error("disk on fire");
    }
    void flush() override { throw std::runtime_error("flush failed"); }
};

int main() {
    for (auto mode : {c_log::Logger::Mode::Sync, c_log::Logger::Mode::Async}) {
        std::vector<std::string> good;
        std::vector<std::string> errors;
        {
            c_log::Logger log(test::quiet_options(mode));
            log.set_error_handler([&](const char* what) { errors.emplace_back(what); });
            log.add_sink<ThrowingSink>();
            log.add_sink<test::CaptureSink>(&good);
            for (int i = 0; i < 10; ++i) log.info("n").kv("i", i);
            log.flush();  // ThrowingSink::flush throws too
            CHECK(log.sink_errors() >= 5);
        }
        // The healthy sink received everything, in order.
        CHECK(good.size() == 10);
        CHECK_CONTAINS(good[9], "\"i\":9");
        // 5 log failures + at least one flush failure (flush() and destructor).
        CHECK(errors.size() >= 6);
        CHECK(errors[0] == "disk on fire");
        CHECK(errors.back() == "flush failed");
    }

    // A handler that itself throws is contained.
    {
        c_log::Logger log(test::quiet_options());
        log.set_error_handler([](const char*) { throw 42; });
        log.add_sink<ThrowingSink>();
        log.info("a");
        log.info("b");
        CHECK(log.sink_errors() == 1);
    }
    return test::result();
}
