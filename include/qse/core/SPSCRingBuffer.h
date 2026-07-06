#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace qse {

/**
 * @brief Single-producer single-consumer lock-free ring buffer (G2).
 *
 * Exactly one thread may call try_push (the producer) and exactly one thread
 * may call try_pop (the consumer). Neither ever blocks the other: each side
 * owns its own monotonic index and only *reads* the other's, so a stalled
 * strategy thread can never freeze the network thread feeding it.
 *
 * Hardware-sympathy details, in order of importance:
 *
 *  - **False sharing.** Cores pull memory in 64-byte cache lines. If the
 *    producer's write index and the consumer's read index shared a line,
 *    every update on one core would invalidate the other core's L1 copy and
 *    force a coherence round-trip. `alignas(64)` pins each index (and each
 *    side's cached view of the opposite index) to its own line.
 *
 *  - **Cached opposite index.** The producer only needs the consumer's index
 *    to detect a full ring, so it keeps a stale local copy and refreshes it
 *    (one acquire load) only when the ring *looks* full - and vice versa for
 *    the consumer. In steady state each side touches only its own lines.
 *
 *  - **Memory ordering.** The slot write must be visible before the index
 *    that publishes it: store-release on the owner's index, load-acquire on
 *    the opposite index, relaxed loads on the owner's own index (it is the
 *    only writer). No seq_cst, no fences, no locks.
 *
 * Indices are unbounded counters masked into the slot array, so capacity
 * must be a power of two and all slots are usable (full == w - r == N).
 */
template <typename T> class SPSCRingBuffer {
public:
    explicit SPSCRingBuffer(std::size_t capacity)
        : capacity_(capacity), mask_(capacity - 1), slots_(capacity) {
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("SPSCRingBuffer capacity must be a power of two >= 2");
        }
    }

    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    /// Producer thread only. Returns false when the ring is full.
    bool try_push(T value) {
        const std::size_t w = write_index_.load(std::memory_order_relaxed);
        if (w - cached_read_index_ == capacity_) {
            cached_read_index_ = read_index_.load(std::memory_order_acquire);
            if (w - cached_read_index_ == capacity_) {
                return false; // genuinely full
            }
        }
        slots_[w & mask_] = std::move(value);
        write_index_.store(w + 1, std::memory_order_release);
        return true;
    }

    /// Consumer thread only. Returns false when the ring is empty.
    bool try_pop(T& out) {
        const std::size_t r = read_index_.load(std::memory_order_relaxed);
        if (r == cached_write_index_) {
            cached_write_index_ = write_index_.load(std::memory_order_acquire);
            if (r == cached_write_index_) {
                return false; // genuinely empty
            }
        }
        out = std::move(slots_[r & mask_]);
        read_index_.store(r + 1, std::memory_order_release);
        return true;
    }

    /// Consumer thread only: pops everything currently available in one
    /// batch. The write index is sampled once (one acquire) and the read
    /// index published once (one release), so the cross-core coherence cost
    /// is amortized over the whole batch instead of paid per item - this is
    /// where the ring beats a locked queue on throughput, not item-at-a-time
    /// chase mode where both are bound by the same cache-line round-trip.
    template <typename Handler> std::size_t consume_all(Handler&& handler) {
        const std::size_t r = read_index_.load(std::memory_order_relaxed);
        const std::size_t w = write_index_.load(std::memory_order_acquire);
        cached_write_index_ = w;
        for (std::size_t i = r; i != w; ++i) {
            handler(std::move(slots_[i & mask_]));
        }
        const std::size_t n = w - r;
        if (n != 0) {
            read_index_.store(w, std::memory_order_release);
        }
        return n;
    }

    std::size_t capacity() const noexcept { return capacity_; }

    /// Approximate (racy by nature); exact only when both threads are quiet.
    std::size_t size_approx() const noexcept {
        return write_index_.load(std::memory_order_acquire) -
               read_index_.load(std::memory_order_acquire);
    }

    bool empty_approx() const noexcept { return size_approx() == 0; }

private:
    // Producer's cache line: its own index + its stale view of the reader
    alignas(64) std::atomic<std::size_t> write_index_{0};
    alignas(64) std::size_t cached_read_index_ = 0;

    // Consumer's cache line: its own index + its stale view of the writer
    alignas(64) std::atomic<std::size_t> read_index_{0};
    alignas(64) std::size_t cached_write_index_ = 0;

    alignas(64) const std::size_t capacity_;
    const std::size_t mask_;
    std::vector<T> slots_;
};

} // namespace qse
