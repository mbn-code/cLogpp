// cLog++ declares a handful of Win32 functions itself instead of including
// <windows.h>. Those declarations must coexist with <windows.h> in either
// order, with and without WIN32_LEAN_AND_MEAN. On other platforms this test
// only checks that the umbrella header is self-contained.
#if defined(_WIN32)
#    include <windows.h>
#endif

#include "test_util.hpp"

#if defined(_WIN32)
// Second include after clogpp, with the lean macro set, exercising the
// opposite order in the same translation unit.
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

int main() {
    std::vector<std::string> lines;
    {
        auto o = test::quiet_options();
        o.thread_id = true;
        c_log::Logger log(o);
        log.add_sink<test::CaptureSink>(&lines);
        c_log::ScopedContext ctx;
        ctx.kv("k", 1);
        log.info("windows.headers");
    }
    CHECK(lines.size() == 1);
    CHECK_CONTAINS(lines[0], "\"k\":1,\"tid\":");
#if defined(_WIN32)
    CHECK(c_log::current_thread_id() == static_cast<std::uint64_t>(::GetCurrentThreadId()));
#endif
    return test::result();
}
