// Arena allocator microbenchmark (roadmap G1). Two experiments:
//
//   1. Raw allocation path: N small allocations through the arena bump
//      pointer vs paired ::operator new/delete calls.
//   2. Order-book workload: build a full-depth book (levels + FIFO orders),
//      walk it with fill_market, destroy it - heap-backed vs arena-backed
//      with an O(1) reset between iterations. This mirrors the inner loop of
//      impact_sweep/ab_audit, where thousands of books are built per run.
//
// Results are recorded in docs/benchmarks/04_arena_allocator.md.

#include "qse/core/Arena.h"
#include "qse/data/OrderBookFullDepth.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Prevents the optimizer from deleting the allocation loops
volatile std::uintptr_t g_sink = 0;

void bench_raw_allocations(std::size_t n) {
    const std::size_t sizes[] = {16, 32, 48, 64};

    // Heap: paired new/delete, freed in bulk afterwards like the arena path
    std::vector<void*> ptrs;
    ptrs.reserve(n);
    auto heap_start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* p = ::operator new(sizes[i % 4]);
        g_sink += reinterpret_cast<std::uintptr_t>(p);
        ptrs.push_back(p);
    }
    for (void* p : ptrs) {
        ::operator delete(p);
    }
    double heap_ms = ms_since(heap_start);

    // Arena: bump for every allocation, one reset at the end
    qse::Arena arena(n * 64 + 1024);
    auto arena_start = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* p = arena.allocate(sizes[i % 4], 8);
        g_sink += reinterpret_cast<std::uintptr_t>(p);
    }
    arena.reset();
    double arena_ms = ms_since(arena_start);

    std::cout << "raw allocations (" << n << " x 16-64B):\n"
              << "  new/delete: " << heap_ms << " ms  (" << heap_ms * 1e6 / n << " ns/op)\n"
              << "  arena:      " << arena_ms << " ms  (" << arena_ms * 1e6 / n << " ns/op)\n"
              << "  speedup:    " << heap_ms / arena_ms << "x\n";
}

std::int64_t build_and_fill_book(qse::OrderBookFullDepth& book, std::size_t levels) {
    for (std::size_t lvl = 0; lvl < levels; ++lvl) {
        book.enqueue_order(qse::Order::Side::SELL, 100.0 + static_cast<double>(lvl) * 0.01,
                           "lvl_" + std::to_string(lvl), 100);
    }
    auto result = book.fill_market(qse::Order::Side::BUY, static_cast<std::int64_t>(levels) * 50);
    return result.first;
}

void bench_book_workload(std::size_t iterations, std::size_t levels) {
    std::int64_t filled_total = 0;

    auto heap_start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        qse::OrderBookFullDepth book;
        filled_total += build_and_fill_book(book, levels);
    }
    double heap_ms = ms_since(heap_start);

    qse::Arena arena(64u << 20); // 64 MiB, reset every iteration
    auto arena_start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        {
            qse::OrderBookFullDepth book(&arena);
            filled_total += build_and_fill_book(book, levels);
        }
        arena.reset();
    }
    double arena_ms = ms_since(arena_start);

    g_sink += static_cast<std::uintptr_t>(filled_total);
    std::cout << "order-book workload (" << iterations << " books x " << levels
              << " levels, build + VWAP walk + destroy):\n"
              << "  heap-backed:  " << heap_ms << " ms  (" << heap_ms * 1e3 / iterations
              << " us/book)\n"
              << "  arena-backed: " << arena_ms << " ms  (" << arena_ms * 1e3 / iterations
              << " us/book)\n"
              << "  speedup:      " << heap_ms / arena_ms << "x\n"
              << "  arena high-water mark: " << arena.high_water_mark() / 1024 << " KiB\n";
}

} // namespace

int main(int argc, char** argv) {
    std::size_t raw_n = 1'000'000;
    std::size_t iterations = 2000;
    std::size_t levels = 200;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        if (flag == "--raw-n")
            raw_n = std::stoul(argv[i + 1]);
        else if (flag == "--iterations")
            iterations = std::stoul(argv[i + 1]);
        else if (flag == "--levels")
            levels = std::stoul(argv[i + 1]);
        else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    bench_raw_allocations(raw_n);
    std::cout << "\n";
    bench_book_workload(iterations, levels);
    return 0;
}
