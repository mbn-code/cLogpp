#pragma once
// Single-producer / single-consumer ring buffer.
//
// As a standalone primitive this is lock-free: one producer thread calling
// push() and one consumer thread calling pop() need no mutex. The Logger,
// however, may have many producer threads (any thread that logs), so it wraps
// this buffer in a mutex to make it multi-producer-safe. See logger.hpp.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace c_log {

template <typename T>
class SPSCRingBuffer {
public:
    // One slot is reserved to distinguish full from empty, so a buffer of
    // `capacity` holds capacity - 1 elements.
    explicit SPSCRingBuffer(std::size_t capacity)
        : capacity_(capacity), buffer_(capacity), head_(0), tail_(0) {
        assert(capacity > 1 && "Capacity must be greater than one");
    }

    // Produce an element. Returns false (and leaves `item` untouched) when the
    // buffer is full.
    bool push(const T& item) { return emplace(item); }
    bool push(T&& item) { return emplace(std::move(item)); }

    // Consume an element, or nullopt when the buffer is empty.
    std::optional<T> pop() {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt;
        std::optional<T> item(std::move(buffer_[tail]));
        tail_.store(next(tail), std::memory_order_release);
        return item;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    bool full() const noexcept {
        return next(head_.load(std::memory_order_acquire)) == tail_.load(std::memory_order_acquire);
    }
    // Number of elements currently stored.
    std::size_t size() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return h >= t ? h - t : capacity_ - t + h;
    }
    // Maximum number of elements the buffer can hold.
    std::size_t capacity() const noexcept { return capacity_ - 1; }

private:
    template <typename U>
    bool emplace(U&& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = next(head);
        if (next_head == tail_.load(std::memory_order_acquire)) return false;  // full
        buffer_[head] = std::forward<U>(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    std::size_t next(std::size_t i) const noexcept { return (i + 1) % capacity_; }

    const std::size_t capacity_;
    std::vector<T> buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

}  // namespace c_log
