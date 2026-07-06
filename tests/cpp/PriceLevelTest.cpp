#include <gtest/gtest.h>
#include "qse/data/OrderBookFullDepth.h"

using namespace qse;

class PriceLevelTest : public ::testing::Test {
protected:
    OrderBookFullDepth book;

    void SetUp() override {
        // Set up any common test data
    }
};

// 6-A.1: Level Struct Test
TEST_F(PriceLevelTest, LevelStruct) {
    Level level(100.0);
    EXPECT_EQ(level.price, 100.0);
    EXPECT_EQ(level.total_size, 0);
    EXPECT_TRUE(level.queue.empty());
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.queue_size(), 0);
}

// 6-A.2: Bid/Ask Maps Test
TEST_F(PriceLevelTest, BidAskMaps) {
    // Test that the class builds and maps are accessible
    EXPECT_TRUE(book.top_n_prices(Order::Side::BUY, 1).empty());
    EXPECT_TRUE(book.top_n_prices(Order::Side::SELL, 1).empty());
}

// 6-A.3: Add Level Helper Test
TEST_F(PriceLevelTest, AddLevel) {
    book.add_level(Order::Side::BUY, 100.0);
    EXPECT_TRUE(book.has_level(Order::Side::BUY, 100.0));

    book.add_level(Order::Side::SELL, 101.0);
    EXPECT_TRUE(book.has_level(Order::Side::SELL, 101.0));

    // Adding same level again should not cause issues
    book.add_level(Order::Side::BUY, 100.0);
    EXPECT_TRUE(book.has_level(Order::Side::BUY, 100.0));
}

// 6-A.4: Remove Level Helper Test
TEST_F(PriceLevelTest, RemoveLevel) {
    // Add a level
    book.add_level(Order::Side::BUY, 100.0);
    EXPECT_TRUE(book.has_level(Order::Side::BUY, 100.0));

    // Remove empty level
    book.remove_level_if_empty(Order::Side::BUY, 100.0);
    EXPECT_FALSE(book.has_level(Order::Side::BUY, 100.0));

    // Try to remove non-existent level (should not crash)
    book.remove_level_if_empty(Order::Side::BUY, 200.0);
}

// 6-A.5: Query Levels Test
TEST_F(PriceLevelTest, TopNPrices) {
    // Add multiple bid levels
    book.add_level(Order::Side::BUY, 100.0);
    book.add_level(Order::Side::BUY, 99.0);
    book.add_level(Order::Side::BUY, 98.0);

    // Add multiple ask levels
    book.add_level(Order::Side::SELL, 101.0);
    book.add_level(Order::Side::SELL, 102.0);
    book.add_level(Order::Side::SELL, 103.0);

    // Test top 3 bid prices (should be in descending order)
    auto bid_prices = book.top_n_prices(Order::Side::BUY, 3);
    ASSERT_EQ(bid_prices.size(), 3);
    EXPECT_EQ(bid_prices[0], 100.0); // Best bid
    EXPECT_EQ(bid_prices[1], 99.0);
    EXPECT_EQ(bid_prices[2], 98.0);

    // Test top 3 ask prices (should be in ascending order)
    auto ask_prices = book.top_n_prices(Order::Side::SELL, 3);
    ASSERT_EQ(ask_prices.size(), 3);
    EXPECT_EQ(ask_prices[0], 101.0); // Best ask
    EXPECT_EQ(ask_prices[1], 102.0);
    EXPECT_EQ(ask_prices[2], 103.0);

    // Test requesting more prices than available
    auto more_bids = book.top_n_prices(Order::Side::BUY, 10);
    EXPECT_EQ(more_bids.size(), 3); // Should only return available prices
}

// 6-A.6: Order Enqueue/Dequeue Test
TEST_F(PriceLevelTest, EnqueueDequeue) {
    // Add a level
    book.add_level(Order::Side::BUY, 100.0);

    // Enqueue orders
    book.enqueue_order(Order::Side::BUY, 100.0, "order1", 100);
    book.enqueue_order(Order::Side::BUY, 100.0, "order2", 200);
    book.enqueue_order(Order::Side::BUY, 100.0, "order3", 300);

    // Check queue positions
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 1); // Head
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order2"), 2);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 3); // Tail

    // Dequeue head
    OrderId dequeued = book.dequeue_head(Order::Side::BUY, 100.0);
    EXPECT_EQ(dequeued, "order1");

    // Check new queue positions
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order1"), 0); // Not found
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order2"), 1); // New head
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 100.0, "order3"), 2);

    // Dequeue more
    EXPECT_EQ(book.dequeue_head(Order::Side::BUY, 100.0), "order2");
    EXPECT_EQ(book.dequeue_head(Order::Side::BUY, 100.0), "order3");

    // Queue should be empty now
    EXPECT_EQ(book.dequeue_head(Order::Side::BUY, 100.0), "");
}

// Additional test for top_of_book functionality
TEST_F(PriceLevelTest, TopOfBook) {
    // Add levels with orders
    book.enqueue_order(Order::Side::BUY, 100.0, "bid1", 100);
    book.enqueue_order(Order::Side::BUY, 99.0, "bid2", 200);
    book.enqueue_order(Order::Side::SELL, 101.0, "ask1", 150);
    book.enqueue_order(Order::Side::SELL, 102.0, "ask2", 250);

    TopOfBook tob = book.top_of_book();
    EXPECT_EQ(tob.best_bid_price, 100.0);
    EXPECT_EQ(tob.best_bid_size, 100);
    EXPECT_EQ(tob.best_ask_price, 101.0);
    EXPECT_EQ(tob.best_ask_size, 150);
    EXPECT_TRUE(tob.has_bid());
    EXPECT_TRUE(tob.has_ask());
    EXPECT_NEAR(tob.mid_price(), 100.5, 0.001);
    EXPECT_NEAR(tob.spread(), 1.0, 0.001);
}

// Test for empty order book
TEST_F(PriceLevelTest, EmptyOrderBook) {
    TopOfBook tob = book.top_of_book();
    EXPECT_EQ(tob.best_bid_price, 0.0);
    EXPECT_EQ(tob.best_bid_size, 0);
    EXPECT_EQ(tob.best_ask_price, 0.0);
    EXPECT_EQ(tob.best_ask_size, 0);
    EXPECT_FALSE(tob.has_bid());
    EXPECT_FALSE(tob.has_ask());
}