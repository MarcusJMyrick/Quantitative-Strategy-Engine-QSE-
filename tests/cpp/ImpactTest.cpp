#include <gtest/gtest.h>
#include "qse/data/OrderBookFullDepth.h"

using namespace qse;

class ImpactTest : public ::testing::Test {
protected:
    OrderBookFullDepth book;

    void SetUp() override { OrderBookFullDepth::reset_queue_id_counter(); }
};

// 6-C.1: Simple Fill Test
TEST_F(ImpactTest, SimpleFill) {
    // Add one level with 500 shares at $10
    book.enqueue_order(Order::Side::SELL, 10.0, "order1", 500);

    // Fill market order for 300 shares
    auto result = book.fill_market(Order::Side::BUY, 300);
    Volume filled = result.first;
    Price avg_price = result.second;

    EXPECT_EQ(filled, 300);
    EXPECT_EQ(avg_price, 10.0);

    // Check that level now has 200 shares remaining
    EXPECT_EQ(book.top_of_book().best_ask_size, 200);
    EXPECT_EQ(book.top_of_book().best_ask_price, 10.0);
}

// 6-C.2: Walk Depth Test
TEST_F(ImpactTest, WalkDepth) {
    // Add two levels: 500@10, 300@9
    book.enqueue_order(Order::Side::BUY, 10.0, "bid1", 500);
    book.enqueue_order(Order::Side::BUY, 9.0, "bid2", 300);

    // Fill market SELL order for 600 shares
    auto result = book.fill_market(Order::Side::SELL, 600);
    Volume filled = result.first;
    Price avg_price = result.second;

    EXPECT_EQ(filled, 600);
    // VWAP = (500*10 + 100*9) / 600 = (5000 + 900) / 600 = 5900 / 600 = 9.833...
    EXPECT_NEAR(avg_price, 9.833333, 0.001);

    // Check that first level is empty and second level has 200 remaining
    EXPECT_EQ(book.top_of_book().best_bid_size, 200);
    EXPECT_EQ(book.top_of_book().best_bid_price, 9.0);
}

// 6-C.3: VWAP Slippage Test
TEST_F(ImpactTest, VWAPSlippage) {
    // Add multiple levels with different prices
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 100);
    book.enqueue_order(Order::Side::SELL, 10.5, "ask2", 200);
    book.enqueue_order(Order::Side::SELL, 11.0, "ask3", 300);

    // Fill market BUY order for 400 shares
    auto result = book.fill_market(Order::Side::BUY, 400);
    Volume filled = result.first;
    Price avg_price = result.second;

    EXPECT_EQ(filled, 400);
    // VWAP = (100*10 + 200*10.5 + 100*11) / 400 = (1000 + 2100 + 1100) / 400 = 4200 / 400 = 10.5
    EXPECT_NEAR(avg_price, 10.5, 0.001);

    // Verify slippage: average price is higher than best ask
    EXPECT_GT(avg_price, 10.0);
}

// 6-C.4: Partial Fill Test
TEST_F(ImpactTest, PartialFill) {
    // Add limited liquidity: 300@10, 200@11
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 300);
    book.enqueue_order(Order::Side::SELL, 11.0, "ask2", 200);

    // Request 1000 shares but only 500 available
    auto result = book.fill_market(Order::Side::BUY, 1000);
    Volume filled = result.first;
    Price avg_price = result.second;

    EXPECT_EQ(filled, 500); // Only 500 filled
    // VWAP = (300*10 + 200*11) / 500 = (3000 + 2200) / 500 = 5200 / 500 = 10.4
    EXPECT_NEAR(avg_price, 10.4, 0.001);

    // Check that book is now empty
    EXPECT_EQ(book.top_of_book().best_ask_size, 0);
}

// Additional edge cases
TEST_F(ImpactTest, EmptyBook) {
    // Try to fill when book is empty
    auto result = book.fill_market(Order::Side::BUY, 100);
    EXPECT_EQ(result.first, 0);
    EXPECT_EQ(result.second, 0.0);
}

TEST_F(ImpactTest, ZeroQuantity) {
    // Add some liquidity
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 100);

    // Try to fill zero quantity
    auto result = book.fill_market(Order::Side::BUY, 0);
    EXPECT_EQ(result.first, 0);
    EXPECT_EQ(result.second, 0.0);
}

TEST_F(ImpactTest, NegativeQuantity) {
    // Add some liquidity
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 100);

    // Try to fill negative quantity
    auto result = book.fill_market(Order::Side::BUY, -50);
    EXPECT_EQ(result.first, 0);
    EXPECT_EQ(result.second, 0.0);
}

TEST_F(ImpactTest, ExactFill) {
    // Add exactly 500 shares
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 500);

    // Fill exactly 500 shares
    auto result = book.fill_market(Order::Side::BUY, 500);
    EXPECT_EQ(result.first, 500);
    EXPECT_EQ(result.second, 10.0);

    // Check that book is now empty
    EXPECT_EQ(book.top_of_book().best_ask_size, 0);
}

TEST_F(ImpactTest, MultipleLevelsExactFill) {
    // Add multiple levels
    book.enqueue_order(Order::Side::BUY, 10.0, "bid1", 100);
    book.enqueue_order(Order::Side::BUY, 9.5, "bid2", 200);
    book.enqueue_order(Order::Side::BUY, 9.0, "bid3", 300);

    // Fill exactly 600 shares (consumes all levels)
    auto result = book.fill_market(Order::Side::SELL, 600);
    EXPECT_EQ(result.first, 600);
    // VWAP = (100*10 + 200*9.5 + 300*9) / 600 = (1000 + 1900 + 2700) / 600 = 5600 / 600 = 9.333...
    EXPECT_NEAR(result.second, 9.333333, 0.001);

    // Check that book is now empty
    EXPECT_EQ(book.top_of_book().best_bid_size, 0);
}

TEST_F(ImpactTest, SingleOrderFill) {
    // Add a single order
    book.enqueue_order(Order::Side::SELL, 10.0, "ask1", 100);

    // Fill less than the order size
    auto result = book.fill_market(Order::Side::BUY, 50);
    EXPECT_EQ(result.first, 50);
    EXPECT_EQ(result.second, 10.0);

    // Check that order still has 50 remaining
    EXPECT_EQ(book.top_of_book().best_ask_size, 50);
}