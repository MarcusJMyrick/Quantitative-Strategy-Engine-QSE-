// SPSC ring buffer throughput benchmark (roadmap G2): moves N items from a
// producer thread to a consumer thread through
//   1. qse::SPSCRingBuffer (lock-free, alignas(64), acquire/release), and
//   2. a std::mutex-guarded std::queue - the "just add a lock" baseline.
// Results are recorded in docs/benchmarks/05_spsc_ring_buffer.md.

#include "qse/core/SPSCRingBuffer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

// Hint the scheduler onto performance cores (Apple Silicon has E-cores that
// would otherwise make numbers noisy); no-op elsewhere
void pin_to_performance_core() {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

double run_spsc(std::uint64_t items, std::size_t capacity, bool batched) {
    qse::SPSCRingBuffer<std::uint64_t> ring(capacity);
    auto start = Clock::now();

    std::thread producer([&] {
        pin_to_performance_core();
        for (std::uint64_t i = 0; i < items; ++i) {
            while (!ring.try_push(i)) {
            }
        }
    });

    pin_to_performance_core();
    std::uint64_t checksum = 0;
    std::uint64_t received = 0;
    while (received < items) {
        if (batched) {
            received += ring.consume_all([&checksum](std::uint64_t&& value) { checksum += value; });
        } else {
            std::uint64_t value = 0;
            if (ring.try_pop(value)) {
                checksum += value;
                ++received;
            }
        }
    }
    producer.join();

    double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (checksum != items * (items - 1) / 2) {
        std::cerr << "SPSC checksum mismatch!\n";
        std::exit(1);
    }
    return ms;
}

double run_mutex_queue(std::uint64_t items, std::size_t capacity) {
    std::queue<std::uint64_t> queue;
    std::mutex mutex;
    auto start = Clock::now();

    std::thread producer([&] {
        pin_to_performance_core();
        for (std::uint64_t i = 0; i < items; ++i) {
            for (;;) {
                std::lock_guard<std::mutex> lock(mutex);
                if (queue.size() < capacity) {
                    queue.push(i);
                    break;
                }
            }
        }
    });

    pin_to_performance_core();

    std::uint64_t checksum = 0;
    std::uint64_t received = 0;
    while (received < items) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!queue.empty()) {
            checksum += queue.front();
            queue.pop();
            ++received;
        }
    }
    producer.join();

    double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    if (checksum != items * (items - 1) / 2) {
        std::cerr << "mutex-queue checksum mismatch!\n";
        std::exit(1);
    }
    return ms;
}

// ~200ns of simulated per-tick strategy work
void strategy_work() {
    const auto until = Clock::now() + std::chrono::nanoseconds(200);
    while (Clock::now() < until) {
    }
}

struct LatencyStats {
    double p50_ns, p99_ns, max_ns, wall_ms;
};

LatencyStats percentiles(std::vector<std::uint32_t>& samples, double wall_ms) {
    auto nth = [&samples](double q) {
        auto it = samples.begin() + static_cast<std::ptrdiff_t>(q * (samples.size() - 1));
        std::nth_element(samples.begin(), it, samples.end());
        return static_cast<double>(*it);
    };
    LatencyStats s{};
    s.p50_ns = nth(0.50);
    s.p99_ns = nth(0.99);
    s.max_ns = static_cast<double>(*std::max_element(samples.begin(), samples.end()));
    s.wall_ms = wall_ms;
    return s;
}

// The jitter experiment: the consumer does ~200ns of strategy work per item.
// In the locked design the work happens INSIDE the critical section (the
// "strategy holds the order-book lock" architecture), so every producer push
// can stall behind it. In the ring design the work happens after the pop and
// the producer never waits on the consumer. Each push is timed individually.
LatencyStats run_ring_latency(std::uint64_t items, std::size_t capacity) {
    qse::SPSCRingBuffer<std::uint64_t> ring(capacity);
    std::vector<std::uint32_t> push_ns(items);
    auto start = Clock::now();

    std::thread consumer([&] {
        pin_to_performance_core();
        std::uint64_t received = 0;
        while (received < items) {
            std::uint64_t value = 0;
            if (ring.try_pop(value)) {
                strategy_work();
                ++received;
            }
        }
    });

    pin_to_performance_core();
    // Ticks arrive on the market's schedule, not back-to-back: pace pushes
    // at ~400ns spacing (below the consumer's capacity) and time ONLY the
    // push - the metric is hand-off jitter, not backpressure
    auto next_arrival = Clock::now();
    for (std::uint64_t i = 0; i < items; ++i) {
        next_arrival += std::chrono::nanoseconds(400);
        while (Clock::now() < next_arrival) {
        }
        auto t0 = Clock::now();
        while (!ring.try_push(i)) {
        }
        push_ns[i] =
            static_cast<std::uint32_t>(std::chrono::nanoseconds(Clock::now() - t0).count());
    }
    consumer.join();
    double wall = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return percentiles(push_ns, wall);
}

LatencyStats run_mutex_latency(std::uint64_t items, std::size_t capacity) {
    std::queue<std::uint64_t> queue;
    std::mutex mutex;
    std::vector<std::uint32_t> push_ns(items);
    auto start = Clock::now();

    std::thread consumer([&] {
        pin_to_performance_core();
        std::uint64_t received = 0;
        while (received < items) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!queue.empty()) {
                queue.pop();
                strategy_work(); // work under the lock: the shared-state design
                ++received;
            }
        }
    });

    pin_to_performance_core();
    auto next_arrival = Clock::now();
    for (std::uint64_t i = 0; i < items; ++i) {
        next_arrival += std::chrono::nanoseconds(400);
        while (Clock::now() < next_arrival) {
        }
        auto t0 = Clock::now();
        for (;;) {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.size() < capacity) {
                queue.push(i);
                break;
            }
        }
        push_ns[i] =
            static_cast<std::uint32_t>(std::chrono::nanoseconds(Clock::now() - t0).count());
    }
    consumer.join();
    double wall = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return percentiles(push_ns, wall);
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t items = 10'000'000;
    std::size_t capacity = 16384;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        if (flag == "--items")
            items = std::stoull(argv[i + 1]);
        else if (flag == "--capacity")
            capacity = std::stoul(argv[i + 1]);
        else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    const double spsc_ms = run_spsc(items, capacity, /*batched=*/false);
    const double batched_ms = run_spsc(items, capacity, /*batched=*/true);
    const double mutex_ms = run_mutex_queue(items, capacity);

    auto rate = [items](double ms) { return static_cast<double>(items) / ms / 1e3; }; // M items/s
    std::cout << "cross-thread hand-off (" << items << " items, capacity " << capacity << "):\n"
              << "  mutex+std::queue:        " << mutex_ms << " ms  (" << rate(mutex_ms)
              << " M items/s)\n"
              << "  SPSC ring (try_pop):     " << spsc_ms << " ms  (" << rate(spsc_ms)
              << " M items/s)  " << mutex_ms / spsc_ms << "x\n"
              << "  SPSC ring (consume_all): " << batched_ms << " ms  (" << rate(batched_ms)
              << " M items/s)  " << mutex_ms / batched_ms << "x\n";

    // The jitter experiment: producer push latency while the consumer does
    // ~200ns of strategy work per item
    const std::uint64_t lat_items = items / 5;
    auto ring_lat = run_ring_latency(lat_items, capacity);
    auto mutex_lat = run_mutex_latency(lat_items, capacity);
    std::cout << "\nproducer push latency with a working consumer (" << lat_items
              << " items, ~200ns work/item):\n"
              << "  locked queue (work under lock): p50 " << mutex_lat.p50_ns << " ns, p99 "
              << mutex_lat.p99_ns << " ns, max " << mutex_lat.max_ns / 1000.0 << " us  (wall "
              << mutex_lat.wall_ms << " ms)\n"
              << "  SPSC ring (work after pop):     p50 " << ring_lat.p50_ns << " ns, p99 "
              << ring_lat.p99_ns << " ns, max " << ring_lat.max_ns / 1000.0 << " us  (wall "
              << ring_lat.wall_ms << " ms)\n";
    return 0;
}
