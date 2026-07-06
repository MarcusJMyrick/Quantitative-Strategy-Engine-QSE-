// G2: SPSC ring buffer semantics (empty/full/wraparound/FIFO), a two-thread
// stress test with ordering + checksum verification, and the live pipeline
// hand-off (ZeroMQ network thread -> ring -> consumer).

#include <gtest/gtest.h>
#include "qse/core/SPSCRingBuffer.h"
#include "qse/messaging/LiveTickPipeline.h"
#include "qse/messaging/TickPublisher.h"

#include <chrono>
#include <cstdint>
#include <thread>

using qse::SPSCRingBuffer;

TEST(SPSCRingBufferTest, CapacityMustBePowerOfTwo) {
    EXPECT_THROW(SPSCRingBuffer<int>(0), std::invalid_argument);
    EXPECT_THROW(SPSCRingBuffer<int>(1), std::invalid_argument);
    EXPECT_THROW(SPSCRingBuffer<int>(100), std::invalid_argument);
    EXPECT_NO_THROW(SPSCRingBuffer<int>(128));
}

TEST(SPSCRingBufferTest, PopOnEmptyFails) {
    SPSCRingBuffer<int> ring(8);
    int value = -1;
    EXPECT_FALSE(ring.try_pop(value));
    EXPECT_TRUE(ring.empty_approx());
}

TEST(SPSCRingBufferTest, PushOnFullFailsUntilPop) {
    SPSCRingBuffer<int> ring(4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ring.try_push(i));
    }
    EXPECT_FALSE(ring.try_push(99)); // full: all 4 slots used
    int value = -1;
    EXPECT_TRUE(ring.try_pop(value));
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(ring.try_push(99)); // one slot free again
}

TEST(SPSCRingBufferTest, FifoOrderAcrossWraparound) {
    SPSCRingBuffer<int> ring(8);
    // Push/pop 5x capacity so the indices wrap the mask repeatedly
    int next_expected = 0;
    int next_value = 0;
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 4; ++i) {
            ASSERT_TRUE(ring.try_push(next_value++));
        }
        int out = -1;
        for (int i = 0; i < 4; ++i) {
            ASSERT_TRUE(ring.try_pop(out));
            ASSERT_EQ(out, next_expected++);
        }
    }
    EXPECT_TRUE(ring.empty_approx());
}

TEST(SPSCRingBufferTest, TwoThreadStressOrderingAndChecksum) {
    // 10M items through a small ring forces constant wraparound and
    // full/empty contention on both sides
    constexpr std::uint64_t kItems = 10'000'000;
    SPSCRingBuffer<std::uint64_t> ring(1024);

    std::thread producer([&ring] {
        for (std::uint64_t i = 0; i < kItems; ++i) {
            while (!ring.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t checksum = 0;
    std::uint64_t expected_next = 0;
    bool order_ok = true;
    std::uint64_t received = 0;
    while (received < kItems) {
        std::uint64_t value = 0;
        if (ring.try_pop(value)) {
            order_ok = order_ok && (value == expected_next);
            ++expected_next;
            checksum += value;
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
    producer.join();

    EXPECT_TRUE(order_ok) << "items arrived out of order";
    // sum 0..N-1 = N(N-1)/2
    EXPECT_EQ(checksum, kItems * (kItems - 1) / 2);
    EXPECT_TRUE(ring.empty_approx());
}

TEST(SPSCRingBufferTest, ConsumeAllDrainsBatchInOrder) {
    SPSCRingBuffer<int> ring(8);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(ring.try_push(i));
    }
    std::vector<int> seen;
    std::size_t n = ring.consume_all([&seen](int&& v) { seen.push_back(v); });
    EXPECT_EQ(n, 5u);
    EXPECT_EQ(seen, (std::vector<int>{0, 1, 2, 3, 4}));
    EXPECT_TRUE(ring.empty_approx());
    // Empty batch is a no-op returning zero
    EXPECT_EQ(ring.consume_all([](int&&) {}), 0u);
    // The ring is fully reusable afterwards
    EXPECT_TRUE(ring.try_push(42));
    int out = 0;
    EXPECT_TRUE(ring.try_pop(out));
    EXPECT_EQ(out, 42);
}

TEST(SPSCRingBufferTest, TwoThreadStressBatchedConsume) {
    constexpr std::uint64_t kItems = 10'000'000;
    SPSCRingBuffer<std::uint64_t> ring(1024);

    std::thread producer([&ring] {
        for (std::uint64_t i = 0; i < kItems; ++i) {
            while (!ring.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t checksum = 0;
    std::uint64_t expected_next = 0;
    bool order_ok = true;
    std::uint64_t received = 0;
    while (received < kItems) {
        std::size_t n = ring.consume_all([&](std::uint64_t&& value) {
            order_ok = order_ok && (value == expected_next);
            ++expected_next;
            checksum += value;
        });
        if (n == 0) {
            std::this_thread::yield();
        }
        received += n;
    }
    producer.join();

    EXPECT_TRUE(order_ok);
    EXPECT_EQ(checksum, kItems * (kItems - 1) / 2);
}

TEST(SPSCRingBufferTest, CarriesTickPayloadIntact) {
    SPSCRingBuffer<qse::Tick> ring(16);
    qse::Tick tick;
    tick.symbol = "AAPL";
    tick.timestamp = qse::from_unix_ms(1748318400000);
    tick.price = 199.23;
    tick.bid = 199.22;
    tick.ask = 199.24;
    tick.bid_size = 500;
    tick.ask_size = 600;
    tick.volume = 8206;
    ASSERT_TRUE(ring.try_push(tick));

    qse::Tick out;
    ASSERT_TRUE(ring.try_pop(out));
    EXPECT_EQ(out.symbol, "AAPL");
    EXPECT_DOUBLE_EQ(out.price, 199.23);
    EXPECT_DOUBLE_EQ(out.bid, 199.22);
    EXPECT_DOUBLE_EQ(out.ask, 199.24);
    EXPECT_EQ(out.volume, 8206);
}

TEST(SPSCRingBufferTest, LivePipelineDeliversTicksAcrossThreads) {
    const std::string endpoint = "tcp://127.0.0.1:5561";
    qse::TickPublisher publisher(endpoint);
    qse::LiveTickPipeline pipeline(endpoint, 1024);
    pipeline.start();

    // Let the SUB socket finish connecting before publishing (slow joiner)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    constexpr int kTicks = 50;
    for (int i = 0; i < kTicks; ++i) {
        qse::Tick tick;
        tick.symbol = "TEST";
        tick.timestamp = qse::from_unix_ms(1000 + i);
        tick.price = 100.0 + i;
        tick.volume = 10;
        publisher.publish_tick("TICK_DATA", tick);
    }

    // Drain on this (consumer) thread until everything arrived or timeout
    std::vector<qse::Tick> received;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (received.size() < kTicks && std::chrono::steady_clock::now() < deadline) {
        pipeline.drain([&received](const qse::Tick& t) { received.push_back(t); });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    pipeline.stop();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(kTicks));
    for (int i = 0; i < kTicks; ++i) {
        EXPECT_DOUBLE_EQ(received[i].price, 100.0 + i); // FIFO preserved end to end
    }
    EXPECT_EQ(pipeline.dropped_ticks(), 0u);
}
