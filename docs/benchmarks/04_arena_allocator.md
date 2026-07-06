# Benchmark 04 — Arena (Bump) Allocator

*Recorded 2026-07-05 on Apple M-series (arm64, macOS 15, Apple clang, -O3).
Reproduce with `./build/arena_bench` (see `--raw-n/--iterations/--levels`).*

## What was built (G1)

`qse::Arena` ([include/qse/core/Arena.h](../../include/qse/core/Arena.h)) is a
fixed-capacity bump allocator exposed as a `std::pmr::memory_resource`:

- one contiguous block requested from the OS up front;
- each allocation aligns the cursor and bumps it — no heap search, no lock,
  no syscall, deterministic latency (the anti-jitter argument);
- individual deallocation is a deliberate no-op; the whole arena is released
  in one O(1) `reset()`, which also eliminates fragmentation;
- exhaustion throws `std::bad_alloc` — the arena never silently spills to
  the heap; instrumentation (`bytes_used`, `high_water_mark`,
  allocation/deallocation/reset counters) shows how to size it.

`OrderBookFullDepth` now takes an optional memory resource: its price-level
maps, FIFO deques, and lookup tables all allocate from the arena, so a book's
working set packs contiguously for L1/L2 cache locality. The default remains
the global new/delete resource, so nothing changes for existing callers.

## Results

### 1. Raw allocation path (1,000,000 × 16–64 B allocations)

| Allocator | Total | Per op | Speedup |
|---|---|---|---|
| `new`/`delete` | 57.2–69.6 ms | 57–70 ns | — |
| arena bump | 3.5 ms | **3.5 ns** | **16–20×** |

The bump path is an add, a compare, and a store — no free-list search and no
allocator lock. The heap number is also the *best case* for new/delete
(no fragmentation, LIFO-ish frees); real long-running sessions degrade
further, and worst-case heap latency (the jitter that loses trades) is far
above its mean.

### 2. Order-book workload (2,000 books × 200 levels, build + VWAP walk + destroy)

| Backing | Per book | Speedup |
|---|---|---|
| heap (default resource) | 139.2–139.6 µs | — |
| arena + O(1) reset | **57.9–58.6 µs** | **2.38–2.51×** |

This is end-to-end — map inserts, FIFO queues, order-size tables,
`fill_market` VWAP walk, destruction — so allocation is only part of each
iteration, and the workload still runs **2.4× faster**. It mirrors the inner
loop of `impact_sweep` and `ab_audit`, which construct thousands of books per
research run. Arena high-water mark per iteration: **884 KiB** (measured by
the built-in instrumentation — this is how capacity should be chosen).

## Notes

- Numbers are the range over repeated runs; the arena side is stable to
  ~0.1 ns/op while the heap side varies run-to-run — deterministic latency is
  the point, not just the mean.
- `OrderId` is a plain `std::string`; short ids stay in SSO buffers, longer
  ids still heap-allocate. Converting to `std::pmr::string` is the next
  increment if profiling shows it matters.
- Verified by `ArenaTest` (7 cases: alignment on a deliberately misaligned
  cursor, bump contiguity, exhaustion + state survival, no-op deallocate,
  O(1) reset with block reuse, pmr-container integration, and behavioral
  equivalence of an arena-backed vs heap-backed order book).
