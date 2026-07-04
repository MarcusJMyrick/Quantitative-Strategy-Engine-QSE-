#include <gtest/gtest.h>
#include "qse/data/OrderBookFullDepth.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>

using namespace qse;

class QueuePositionTest : public ::testing::Test {
protected:
    OrderBookFullDepth book;
    
    void SetUp() override {
        // Reset the static counter for consistent test behavior
        OrderBookFullDepth::reset_queue_id_counter();
    }
};

// 6-B.1: Queue ID Generation Tests
TEST_F(QueuePositionTest, QueueIdIncrements) {
    QueueId id1 = book.allocate_queue_id();
    QueueId id2 = book.allocate_queue_id();
    QueueId id3 = book.allocate_queue_id();
    
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
    EXPECT_EQ(id3, 3);
    EXPECT_GT(id3, id2);
    EXPECT_GT(id2, id1);
}

TEST_F(QueuePositionTest, QueueIdUniqueness) {
    std::vector<QueueId> ids;
    for (int i = 0; i < 100; ++i) {
        ids.push_back(book.allocate_queue_id());
    }
    
    // Check all IDs are unique
    std::sort(ids.begin(), ids.end());
    auto it = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(it, ids.end()) << "Duplicate queue IDs found";
    
    // Check they are sequential
    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_EQ(ids[i], i + 1) << "Queue ID " << i << " is not sequential";
    }
}

// 6-B.2: Enqueue Order Tests
TEST_F(QueuePositionTest, EnqueueOrder) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
    
    // Check positions are correct
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order2"), 2);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 3);
    
    // Check QueueId-based position lookup
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(id2), 2);
    EXPECT_EQ(book.queue_position(id3), 3);
}

TEST_F(QueuePositionTest, EnqueueOrderBothSides) {
    QueueId bid_id1 = book.enqueue_order(Order::Side::BUY, 100.0, "bid1", 100);
    QueueId bid_id2 = book.enqueue_order(Order::Side::BUY, 100.0, "bid2", 200);
    QueueId ask_id1 = book.enqueue_order(Order::Side::SELL, 101.0, "ask1", 150);
    QueueId ask_id2 = book.enqueue_order(Order::Side::SELL, 101.0, "ask2", 250);
    
    // Check bid positions
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "bid1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "bid2"), 2);
    EXPECT_EQ(book.queue_position(bid_id1), 1);
    EXPECT_EQ(book.queue_position(bid_id2), 2);
    
    // Check ask positions
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 101.0, "ask1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 101.0, "ask2"), 2);
    EXPECT_EQ(book.queue_position(ask_id1), 1);
    EXPECT_EQ(book.queue_position(ask_id2), 2);
    
    // Cross-check: bids don't affect asks and vice versa
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 101.0, "bid1"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "ask1"), 0);
}

TEST_F(QueuePositionTest, EnqueueOrderMultiplePrices) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 99.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    QueueId id4 = book.enqueue_order(Order::Side::BUY, 99.0, "order4", 400);
    
    // Check positions at 100.0
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 2);
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(id3), 2);
    
    // Check positions at 99.0
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 99.0, "order2"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 99.0, "order4"), 2);
    EXPECT_EQ(book.queue_position(id2), 1);
    EXPECT_EQ(book.queue_position(id4), 2);
}

// 6-B.3: Position Lookup API Tests
TEST_F(QueuePositionTest, LookupUnknown) {
    // Test with non-existent QueueId
    EXPECT_EQ(book.queue_position(999999), 0);
    
    // Test with non-existent OrderId
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "nonexistent"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 100.0, "nonexistent"), 0);
    
    // Test with non-existent price level
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 999.0, "any"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 999.0, "any"), 0);
}

TEST_F(QueuePositionTest, PositionLookupPerformance) {
    // Add many orders to test O(1) lookup performance
    std::vector<QueueId> ids;
    for (int i = 0; i < 1000; ++i) {
        std::string order_id = "order" + std::to_string(i);
        ids.push_back(book.enqueue_order(Order::Side::BUY, 100.0, order_id, 100));
    }
    
    // Test position lookups
    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_EQ(book.queue_position(ids[i]), i + 1);
        std::string order_id = "order" + std::to_string(i);
        EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, order_id), i + 1);
    }
}

// 6-B.4: Position Updates on Dequeue/Cancel Tests
TEST_F(QueuePositionTest, AfterDequeue) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    
    // Verify initial positions
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(id2), 2);
    EXPECT_EQ(book.queue_position(id3), 3);
    
    // Dequeue head
    OrderId dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "order1");
    
    // Check updated positions
    EXPECT_EQ(book.queue_position(id1), 0); // Removed
    EXPECT_EQ(book.queue_position(id2), 1); // Moved to head
    EXPECT_EQ(book.queue_position(id3), 2); // Moved up
    
    // Check OrderId-based lookup
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order2"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 2);
}

TEST_F(QueuePositionTest, AfterCancel) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    
    // Cancel middle order
    bool cancelled = book.cancel_order(id2);
    EXPECT_TRUE(cancelled);
    
    // Check updated positions
    EXPECT_EQ(book.queue_position(id1), 1); // Still at head
    EXPECT_EQ(book.queue_position(id2), 0); // Removed
    EXPECT_EQ(book.queue_position(id3), 2); // Moved up
    
    // Check OrderId-based lookup
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order2"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 2);
}

TEST_F(QueuePositionTest, CancelNonExistent) {
    // Try to cancel non-existent QueueId
    bool cancelled = book.cancel_order(999999);
    EXPECT_FALSE(cancelled);
    
    // Add an order and then cancel it
    QueueId id = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    EXPECT_EQ(book.queue_position(id), 1);
    
    cancelled = book.cancel_order(id);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(book.queue_position(id), 0);
    
    // Try to cancel the same ID again
    cancelled = book.cancel_order(id);
    EXPECT_FALSE(cancelled);
}

TEST_F(QueuePositionTest, DequeueEmptyQueue) {
    // Try to dequeue from empty queue
    OrderId dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "");
    
    // Add and remove all orders
    QueueId id = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    EXPECT_EQ(book.queue_position(id), 1);
    
    dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "order1");
    EXPECT_EQ(book.queue_position(id), 0);
    
    // Try to dequeue again
    dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "");
}

// 6-B.5: Cross-price isolation
TEST_F(QueuePositionTest, MultiplePrices) {
    // Enqueue at $10
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 10.0, "orderA1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 10.0, "orderA2", 200);
    // Enqueue at $11
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 11.0, "orderB1", 150);
    QueueId id4 = book.enqueue_order(Order::Side::BUY, 11.0, "orderB2", 250);

    // Check positions at $10
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 10.0, "orderA1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 10.0, "orderA2"), 2);
    // Check positions at $11
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 11.0, "orderB1"), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 11.0, "orderB2"), 2);

    // Cross-check: orders at $10 do not appear at $11 and vice versa
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 11.0, "orderA1"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 10.0, "orderB1"), 0);

    // Check that queue_position(QueueId) works and is isolated
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(id2), 2);
    EXPECT_EQ(book.queue_position(id3), 1);
    EXPECT_EQ(book.queue_position(id4), 2);
}

TEST_F(QueuePositionTest, CrossPriceOperations) {
    // Add orders at different prices
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 99.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    
    // Dequeue from one price level
    OrderId dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "order1");
    
    // Check that other price level is unaffected
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 99.0, "order2"), 1);
    
    // Cancel from other price level
    bool cancelled = book.cancel_order(id2);
    EXPECT_TRUE(cancelled);
    
    // Check that first price level is unaffected
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 1);
}

// Edge Cases and Stress Tests
TEST_F(QueuePositionTest, LargeNumberOfOrders) {
    const int NUM_ORDERS = 10000;
    std::vector<QueueId> ids;
    
    // Add many orders
    for (int i = 0; i < NUM_ORDERS; ++i) {
        std::string order_id = "order" + std::to_string(i);
        ids.push_back(book.enqueue_order(Order::Side::BUY, 100.0, order_id, 100));
    }
    
    // Verify all positions are correct
    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_EQ(book.queue_position(ids[i]), i + 1);
    }
    
    // Cancel every other order
    for (size_t i = 0; i < ids.size(); i += 2) {
        bool cancelled = book.cancel_order(ids[i]);
        EXPECT_TRUE(cancelled);
    }
    
    // Check remaining positions are correct
    for (size_t i = 1; i < ids.size(); i += 2) {
        EXPECT_EQ(book.queue_position(ids[i]), (i / 2) + 1);
    }
}

TEST_F(QueuePositionTest, ConcurrentQueueIdGeneration) {
    std::vector<std::thread> threads;
    std::vector<QueueId> ids;
    std::mutex ids_mutex;
    
    // Spawn multiple threads to allocate queue IDs
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                QueueId id = book.allocate_queue_id();
                std::lock_guard<std::mutex> lock(ids_mutex);
                ids.push_back(id);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check all IDs are unique
    std::sort(ids.begin(), ids.end());
    auto it = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(it, ids.end()) << "Duplicate queue IDs found in concurrent test";
    
    // Check they are sequential
    for (size_t i = 0; i < ids.size(); ++i) {
        EXPECT_EQ(ids[i], i + 1) << "Queue ID " << i << " is not sequential";
    }
}

TEST_F(QueuePositionTest, ZeroSizeOrders) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 0);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 0);
    
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(id2), 2);
}

TEST_F(QueuePositionTest, PriceLevelRemoval) {
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    
    // Remove all orders from the level
    OrderId dequeued1 = book.dequeue_head(Order::Side::BUY, 100.0);
    OrderId dequeued2 = book.dequeue_head(Order::Side::BUY, 100.0);
    
    EXPECT_EQ(dequeued1, "order1");
    EXPECT_EQ(dequeued2, "order2");
    
    // Check positions are 0
    EXPECT_EQ(book.queue_position(id1), 0);
    EXPECT_EQ(book.queue_position(id2), 0);
    
    // Try to dequeue from empty level
    OrderId dequeued3 = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued3, "");
}

TEST_F(QueuePositionTest, MixedOperations) {
    // Add orders
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    QueueId id3 = book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);
    QueueId id4 = book.enqueue_order(Order::Side::BUY, 100.0, "order4", 400);
    
    // Dequeue head
    OrderId dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "order1");
    
    // Cancel middle order
    bool cancelled = book.cancel_order(id3);
    EXPECT_TRUE(cancelled);
    
    // Add new order
    QueueId id5 = book.enqueue_order(Order::Side::BUY, 100.0, "order5", 500);
    
    // Check final positions
    EXPECT_EQ(book.queue_position(id1), 0); // Removed
    EXPECT_EQ(book.queue_position(id2), 1); // Moved to head
    EXPECT_EQ(book.queue_position(id3), 0); // Cancelled
    EXPECT_EQ(book.queue_position(id4), 2); // Moved up
    EXPECT_EQ(book.queue_position(id5), 3); // Added at end
}

TEST_F(QueuePositionTest, BothSidesMixedOperations) {
    // Add orders to both sides
    QueueId bid1 = book.enqueue_order(Order::Side::BUY, 100.0, "bid1", 100);
    QueueId bid2 = book.enqueue_order(Order::Side::BUY, 100.0, "bid2", 200);
    QueueId ask1 = book.enqueue_order(Order::Side::SELL, 101.0, "ask1", 150);
    QueueId ask2 = book.enqueue_order(Order::Side::SELL, 101.0, "ask2", 250);
    
    // Dequeue from bids
    OrderId dequeued_bid = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued_bid, "bid1");
    
    // Cancel from asks
    bool cancelled_ask = book.cancel_order(ask2);
    EXPECT_TRUE(cancelled_ask);
    
    // Check bid positions
    EXPECT_EQ(book.queue_position(bid1), 0); // Removed
    EXPECT_EQ(book.queue_position(bid2), 1); // Moved to head
    
    // Check ask positions
    EXPECT_EQ(book.queue_position(ask1), 1); // Still at head
    EXPECT_EQ(book.queue_position(ask2), 0); // Cancelled
    
    // Cross-check: operations on one side don't affect the other
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "ask1"), 0);
    EXPECT_EQ(book.queue_position(Order::Side::SELL, 101.0, "bid2"), 0);
}

TEST_F(QueuePositionTest, DuplicateOrderIds) {
    // Attempt to enqueue duplicate OrderId at the same price level – should be rejected
    QueueId id1 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    QueueId id2 = book.enqueue_order(Order::Side::BUY, 100.0, "order1", 200);

    EXPECT_NE(id1, 0u);
    EXPECT_EQ(id2, 0u); // Duplicate insertion returns 0 (failure)

    // Position of first order remains intact
    EXPECT_EQ(book.queue_position(id1), 1);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 1);
} 