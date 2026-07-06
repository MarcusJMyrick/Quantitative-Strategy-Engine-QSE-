# 05 — SPSC Lock-Free Ring Buffer (G2)

*Measured 2026-07-06 on Apple M-series (macOS, Apple clang, `-O3` Release);
tool: `build/spsc_bench`, 10M items, ring capacity 16,384. TSan harness:
`build/spsc_tsan_stress` (clang `-fsanitize=thread`).*

## What was built

`qse::SPSCRingBuffer<T>` ([SPSCRingBuffer.h](../../include/qse/core/SPSCRingBuffer.h)):
single-producer/single-consumer, power-of-two capacity, unbounded monotonic
indices masked into the slot array.

- **False sharing defeated:** `alignas(64)` pins the producer's write index,
  the consumer's read index, and each side's *cached copy of the opposite
  index* to separate cache lines.
- **Cached opposite index:** each side re-reads the other's index only when
  the ring looks full/empty, so in steady state each core touches only its
  own lines.
- **Memory ordering:** store-release publishes, load-acquire observes,
  relaxed loads on the owner's own index. No seq_cst, no fences, no locks.
- **`consume_all` batch drain:** one acquire + one release per batch instead
  of per item.
- **Integration:** `qse::LiveTickPipeline` — ZeroMQ subscriber thread pushes
  ticks into the ring, the strategy thread drains at its own pace; overflow
  is dropped *and counted* (visible backpressure instead of a frozen feed).

## Correctness

- 9 gtest cases: empty/full/wraparound/FIFO, batch drain, Tick payload,
  two 10M-item two-thread stress tests (strict ordering + checksum), and an
  end-to-end pipeline test over real ZeroMQ.
- **ThreadSanitizer: clean** over 10M items alternating both consumer paths
  (`try_pop` and `consume_all`): `ordered=1 checksum_ok=1`, no data races.

## Throughput — the honest result

| Hand-off (10M × uint64) | Wall time | Rate | vs mutex |
|---|---|---|---|
| `std::mutex` + `std::queue` | 428 ms | 23.4 M items/s | 1.00× |
| SPSC ring, `try_pop` | 417 ms | 24.0 M items/s | 1.03× |
| SPSC ring, `consume_all` | 451 ms | 22.2 M items/s | 0.95× |

**Throughput parity, and that is the correct result to report.** In
item-at-a-time chase mode (consumer keeping pace with producer), every pop
must observe the producer's freshly written index and slot — a cross-core
cache-line round-trip (~40 ns on M-series) that bounds *both* designs
identically. macOS's `os_unfair_lock`-backed mutex is also exceptionally
cheap when barely contended. Raw mean throughput is not where a lock-free
ring earns its keep.

## Jitter — where the ring earns its keep

The scenario the roadmap describes: the consumer does ~200 ns of strategy
work per tick. In the locked design the work happens inside the critical
section (the "strategy holds the shared-state lock" architecture); in the
ring design it happens after the pop. Producer pushes are paced at 400 ns
(market-driven arrivals, not saturation) and each push is timed individually
— 2M samples:

| Producer push latency | p50 | p99 | max | wall time |
|---|---|---|---|---|
| Locked queue (work under lock) | 41 ns | **16,334 ns** | **1,153 µs** | 1,396 ms |
| SPSC ring (work after pop) | 41 ns | **42 ns** | 71 µs | 800 ms |

Identical medians — and a **389× difference at p99**. The locked feed's
worst case exceeds **a millisecond** (lock-holder preemption: the network
thread sits blocked while the OS deschedules the strategy mid-critical-
section); the ring's push cost is flat because the producer never waits on
the consumer, ever. Wall time also improves 1.75× because production
overlaps with consumption instead of serializing behind the lock.

That flat p99 is the entire argument: market-data hand-off is a tail-latency
problem, and locks have unbounded tails by construction.

## Reproduce

```bash
cmake --build build --target spsc_bench spsc_tsan_stress -j8
./build/spsc_bench            # throughput + jitter experiments
./build/spsc_tsan_stress      # must print ordered=1 checksum_ok=1, no TSan reports
```
