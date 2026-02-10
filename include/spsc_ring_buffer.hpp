#pragma once

#include <atomic>
#include <vector>
#include <optional>
#include <cassert>

// Lock-Free Single-Producer-Single-Consumer (SPSC) Ring Buffer
// Author: cLog contributors

namespace c_log {

// Generic Ring Buffer for SPSC context 
template<typename T>
class SPSCRingBuffer {
public:
    explicit SPSCRingBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity), head_(0), tail_(0) {
        assert(capacity > 0 && "Capacity must be greater than zero");
    }

    // Produces an element. Returns true if successful, false if the buffer is full.
    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) % capacity_;

        // Check if the buffer is full.
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Buffer full.
        }

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumes an element. Returns std::optional<T>.
    std::optional<T> pop() {
        size_t tail = tail_.load(std::memory_order_relaxed);

        // Check if the buffer is empty.
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;  // Buffer empty.
        }

        T item = buffer_[tail];
        tail_.store((tail + 1) % capacity_, std::memory_order_release);
        return item;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

} // namespace c_log