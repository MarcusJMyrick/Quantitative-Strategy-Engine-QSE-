// ThreadSanitizer stress harness for SPSCRingBuffer (roadmap G2). Built with
// -fsanitize=thread via the spsc_tsan_stress CMake target; a clean run
// certifies the acquire/release protocol has no data race.
//
//   cmake --build build --target spsc_tsan_stress && ./build/spsc_tsan_stress

#include "qse/core/SPSCRingBuffer.h"

#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    constexpr std::uint64_t kItems = 10'000'000;
    qse::SPSCRingBuffer<std::uint64_t> ring(1024);

    std::thread producer([&ring] {
        for (std::uint64_t i = 0; i < kItems; ++i) {
            while (!ring.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t checksum = 0;
    std::uint64_t expected = 0;
    std::uint64_t received = 0;
    bool ordered = true;
    // Alternate between the two consumer paths so TSan certifies both the
    // item-at-a-time pop and the batched drain protocol
    bool use_batch = false;
    while (received < kItems) {
        if (use_batch) {
            std::size_t n = ring.consume_all([&](std::uint64_t&& value) {
                ordered = ordered && (value == expected++);
                checksum += value;
            });
            if (n == 0) {
                std::this_thread::yield();
            }
            received += n;
        } else {
            std::uint64_t value = 0;
            if (ring.try_pop(value)) {
                ordered = ordered && (value == expected++);
                checksum += value;
                ++received;
            } else {
                std::this_thread::yield();
            }
        }
        use_batch = !use_batch;
    }
    producer.join();

    const bool checksum_ok = checksum == kItems * (kItems - 1) / 2;
    std::cout << "moved " << received << " items; ordered=" << ordered
              << " checksum_ok=" << checksum_ok << "\n";
    return (ordered && checksum_ok) ? 0 : 1;
}
