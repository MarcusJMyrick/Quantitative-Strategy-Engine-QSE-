#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>

namespace qse {

/**
 * @brief Fixed-capacity bump (arena) allocator, exposed as a
 * std::pmr::memory_resource so standard pmr containers can allocate from it.
 *
 * One contiguous block is requested from the OS up front. Each allocation
 * aligns the cursor and bumps it forward - no heap search, no lock, no
 * syscall, deterministic latency. Individual deallocation is a deliberate
 * no-op (monotonic semantics): the whole arena is released in one O(1)
 * reset() between sessions/backtests, which also eliminates fragmentation.
 * Objects allocated together sit contiguously, so hot-path traversals stay
 * in L1/L2 cache.
 *
 * Exhaustion throws std::bad_alloc - the arena never silently spills to the
 * heap. Size it for the session and monitor high_water_mark().
 *
 * Not thread-safe by design (single-owner hot path); reset() must not run
 * while objects allocated from the arena are still alive.
 */
class Arena : public std::pmr::memory_resource {
public:
    explicit Arena(std::size_t capacity_bytes)
        : buffer_(static_cast<std::byte*>(::operator new(capacity_bytes))),
          capacity_(capacity_bytes) {}

    ~Arena() override { ::operator delete(buffer_); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    /// Discards every allocation in O(1); the next allocation reuses offset 0.
    void reset() noexcept {
        offset_ = 0;
        ++reset_count_;
    }

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t bytes_used() const noexcept { return offset_; }
    std::size_t high_water_mark() const noexcept { return high_water_; }
    std::size_t allocation_count() const noexcept { return allocation_count_; }
    std::size_t deallocation_count() const noexcept { return deallocation_count_; }
    std::size_t reset_count() const noexcept { return reset_count_; }

    /// True if p points into the arena's block (used by tests).
    bool owns(const void* p) const noexcept {
        const auto* b = static_cast<const std::byte*>(p);
        return b >= buffer_ && b < buffer_ + capacity_;
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        // Align the actual address, not the offset: operator new only
        // guarantees 16-byte base alignment and callers may ask for more.
        // The result is derived from buffer_ by pointer arithmetic (not an
        // int-to-pointer cast) so pointer provenance is preserved
        const auto base = reinterpret_cast<std::uintptr_t>(buffer_);
        const std::uintptr_t current = base + offset_;
        const std::uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
        const std::size_t aligned_offset = aligned - base;
        const std::size_t new_offset = aligned_offset + bytes;
        if (new_offset > capacity_) {
            throw std::bad_alloc();
        }
        offset_ = new_offset;
        if (offset_ > high_water_) {
            high_water_ = offset_;
        }
        ++allocation_count_;
        return buffer_ + aligned_offset;
    }

    void do_deallocate(void* /*p*/, std::size_t /*bytes*/, std::size_t /*alignment*/) override {
        // Monotonic: freeing happens wholesale in reset()
        ++deallocation_count_;
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::byte* buffer_;
    std::size_t capacity_;
    std::size_t offset_ = 0;
    std::size_t high_water_ = 0;
    std::size_t allocation_count_ = 0;
    std::size_t deallocation_count_ = 0;
    std::size_t reset_count_ = 0;
};

} // namespace qse
