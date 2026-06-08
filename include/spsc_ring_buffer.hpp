#pragma once

#include <atomic>
#include <vector>
#include <optional>
#include <cassert>

// Single-Producer-Single-Consumer (SPSC) ring buffer.
//
// As a standalone primitive this is lock-free: one producer thread calling
// push() and one consumer thread calling pop() need no mutex. The Logger,
// however, may have many producer threads (any thread that logs), so it wraps
// this buffer in a mutex to make it multi-producer-safe. See logger.hpp.
//
// Author: cLog++ contributors

namespace c_log {

// Generic Ring Buffer for SPSC context
template<typename T>
class SPSCRingBuffer {
public:
    explicit SPSCRingBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity), head_(0), tail_(0) {
        assert(capacity > 1 && "Capacity must be greater than one (one slot is reserved to tell full from empty)");
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

    // True if there is nothing to consume. (Indices are reduced modulo
    // capacity_ on every step, so they always stay in [0, capacity_) and
    // can never overflow.)
    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

} // namespace c_log