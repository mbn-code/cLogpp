// SPSCRingBuffer: capacity accounting, wrap-around, move-only payloads, and a
// producer/consumer thread pair without any external lock.
#include <memory>
#include <thread>

#include "test_util.hpp"

int main() {
    c_log::SPSCRingBuffer<int> rb(4);  // holds 3
    CHECK(rb.capacity() == 3);
    CHECK(rb.empty());
    CHECK(!rb.full());
    CHECK(rb.size() == 0);
    CHECK(!rb.pop().has_value());

    CHECK(rb.push(1));
    CHECK(rb.push(2));
    CHECK(rb.push(3));
    CHECK(rb.full());
    CHECK(rb.size() == 3);
    CHECK(!rb.push(4));  // full
    CHECK(rb.pop() == 1);
    CHECK(rb.size() == 2);
    CHECK(rb.push(4));  // wrap-around
    CHECK(rb.pop() == 2);
    CHECK(rb.pop() == 3);
    CHECK(rb.pop() == 4);
    CHECK(rb.empty());
    CHECK(!rb.pop().has_value());

    // Move-only elements are moved in and out, not copied.
    c_log::SPSCRingBuffer<std::unique_ptr<int>> mv(3);
    auto p = std::make_unique<int>(7);
    CHECK(mv.push(std::move(p)));
    CHECK(p == nullptr);
    auto q = std::make_unique<int>(8);
    CHECK(mv.push(std::move(q)));
    CHECK(!mv.push(std::make_unique<int>(9)));  // full: rvalue not consumed
    std::unique_ptr<int> out = std::move(mv.pop()).value_or(nullptr);
    CHECK(out && *out == 7);
    out = std::move(mv.pop()).value_or(nullptr);
    CHECK(out && *out == 8);
    CHECK(mv.empty());
    CHECK(std::move(mv.pop()).value_or(nullptr) == nullptr);

    // A failed push leaves the rvalue intact.
    c_log::SPSCRingBuffer<std::unique_ptr<int>> tiny(2);
    CHECK(tiny.push(std::make_unique<int>(1)));
    auto keep = std::make_unique<int>(2);
    CHECK(!tiny.push(std::move(keep)));
    CHECK(keep != nullptr && *keep == 2);

    // Lock-free SPSC: one producer, one consumer, all values in order.
    c_log::SPSCRingBuffer<int> chan(64);
    const int N = 200000;
    int mismatches = 0;
    std::thread consumer([&] {
        int expect = 0;
        while (expect < N) {
            auto v = chan.pop();
            if (!v) {
                std::this_thread::yield();
                continue;
            }
            if (*v != expect) ++mismatches;
            ++expect;
        }
    });
    for (int i = 0; i < N; ++i)
        while (!chan.push(i)) std::this_thread::yield();
    consumer.join();
    CHECK(mismatches == 0);
    CHECK(chan.empty());
    return test::result();
}
