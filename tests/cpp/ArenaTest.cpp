// G1: the arena must guarantee alignment, bump contiguously, refuse to spill
// past capacity, reset in O(1), and behave as a drop-in pmr resource for the
// full-depth order book.

#include <gtest/gtest.h>
#include "qse/core/Arena.h"
#include "qse/data/OrderBookFullDepth.h"

#include <cstdint>
#include <memory_resource>
#include <vector>

using qse::Arena;

TEST(ArenaTest, AlignmentIsRespected) {
    Arena arena(4096);
    // Deliberately misalign the cursor, then demand strict alignments
    void* p1 = arena.allocate(1, 1);
    ASSERT_NE(p1, nullptr);
    for (std::size_t alignment : {8u, 16u, 64u, 128u}) {
        void* p = arena.allocate(8, alignment);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % alignment, 0u) << "alignment " << alignment;
        EXPECT_TRUE(arena.owns(p));
    }
}

TEST(ArenaTest, BumpIsContiguous) {
    Arena arena(4096);
    auto* a = static_cast<std::byte*>(arena.allocate(16, 8));
    auto* b = static_cast<std::byte*>(arena.allocate(16, 8));
    // Same alignment, aligned sizes: the second block starts where the
    // first ended - that contiguity is the cache-locality argument
    EXPECT_EQ(b, a + 16);
    EXPECT_EQ(arena.bytes_used(), 32u);
    EXPECT_EQ(arena.allocation_count(), 2u);
}

TEST(ArenaTest, ExhaustionThrowsAndStateSurvives) {
    Arena arena(64);
    void* p = arena.allocate(48, 8);
    ASSERT_NE(p, nullptr);
    EXPECT_THROW(arena.allocate(32, 8), std::bad_alloc);
    // The failed allocation must not corrupt the cursor: the remaining
    // capacity is still usable
    EXPECT_EQ(arena.bytes_used(), 48u);
    EXPECT_NO_THROW(arena.allocate(16, 8));
    EXPECT_EQ(arena.bytes_used(), 64u);
}

TEST(ArenaTest, DeallocateIsANoOp) {
    Arena arena(1024);
    void* p = arena.allocate(64, 8);
    arena.deallocate(p, 64, 8);
    EXPECT_EQ(arena.bytes_used(), 64u); // nothing came back
    EXPECT_EQ(arena.deallocation_count(), 1u);
}

TEST(ArenaTest, ResetReusesTheBlockFromOffsetZero) {
    Arena arena(1024);
    void* first = arena.allocate(64, 8);
    arena.allocate(128, 8);
    EXPECT_EQ(arena.high_water_mark(), 192u);

    arena.reset();
    EXPECT_EQ(arena.bytes_used(), 0u);
    EXPECT_EQ(arena.reset_count(), 1u);

    void* again = arena.allocate(64, 8);
    EXPECT_EQ(again, first); // same block, same start
    // High-water mark survives reset: it reports sizing across the session
    EXPECT_EQ(arena.high_water_mark(), 192u);
}

TEST(ArenaTest, WorksAsPmrResourceForContainers) {
    Arena arena(1 << 16);
    std::pmr::vector<int> v(&arena);
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
    }
    EXPECT_TRUE(arena.owns(v.data()));
    EXPECT_EQ(v[999], 999);
    EXPECT_GE(arena.bytes_used(), 1000 * sizeof(int));
}

TEST(ArenaTest, OrderBookOnArenaMatchesDefaultBook) {
    qse::OrderBookFullDepth::reset_queue_id_counter();
    Arena arena(1 << 20);
    std::size_t before = arena.bytes_used();

    // Same scenario through an arena-backed and a heap-backed book
    auto run = [](qse::OrderBookFullDepth& book) {
        for (int lvl = 0; lvl < 10; ++lvl) {
            book.enqueue_order(qse::Order::Side::SELL, 100.0 + lvl * 0.01,
                               "a" + std::to_string(lvl), 100);
        }
        return book.fill_market(qse::Order::Side::BUY, 550);
    };

    qse::OrderBookFullDepth arena_book(&arena);
    qse::OrderBookFullDepth heap_book;
    auto arena_result = run(arena_book);
    auto heap_result = run(heap_book);

    EXPECT_EQ(arena_result.first, heap_result.first);
    EXPECT_DOUBLE_EQ(arena_result.second, heap_result.second);
    EXPECT_EQ(arena_book.top_of_book().best_ask_size, heap_book.top_of_book().best_ask_size);
    // The book's internals really allocated from the arena
    EXPECT_GT(arena.bytes_used(), before);
}
