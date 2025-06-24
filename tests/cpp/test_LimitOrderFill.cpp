#include <gtest/gtest.h>
#include "qse/order/OrderManager.h"
#include "qse/data/OrderBook.h"
#include "qse/core/Config.h"
#include "qse/data/Data.h"

class LimitOrderFillTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple config
        config_ = std::make_unique<qse::Config>();
        
        // Create order book
        order_book_ = std::make_unique<qse::OrderBook>();
        
        // Create order manager with OrderBook integration
        order_manager_ = std::make_unique<qse::OrderManager>(
            *config_, *order_book_, "test_equity.csv", "test_tradelog.csv"
        );
    }
    
    void TearDown() override {
        // Clean up test files
        std::remove("test_equity.csv");
        std::remove("test_tradelog.csv");
    }
    
    std::unique_ptr<qse::Config> config_;
    std::unique_ptr<qse::OrderBook> order_book_;
    std::unique_ptr<qse::OrderManager> order_manager_;
};

TEST_F(LimitOrderFillTest, PartialFillsWithOrderBook) {
    // Submit a limit buy order for 150 shares at price 100.0
    auto order_id = order_manager_->submit_limit_order(
        "AAPL", qse::Order::Side::BUY, 150, 100.0, qse::Order::TimeInForce::GTC
    );
    
    // Verify order is pending
    auto order = order_manager_->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    EXPECT_EQ(order->filled_quantity, 0);
    
    // First tick: ask=100.0, ask_size=100 - should fill 100 shares
    qse::Tick tick1{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 100, 100, 100};
    order_manager_->process_tick(tick1);
    
    order = order_manager_->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_EQ(order->remaining_quantity(), 50);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0);
    
    // Second tick: ask=100.0, ask_size=100 - should fill remaining 50 shares
    qse::Tick tick2{"AAPL", qse::from_unix_ms(1001), 100.0, 99.5, 100.0, 100, 100, 100};
    order_manager_->process_tick(tick2);
    
    order = order_manager_->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 150);
    EXPECT_EQ(order->remaining_quantity(), 0);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0);
    
    // Verify portfolio was updated correctly
    EXPECT_EQ(order_manager_->get_position("AAPL"), 150);
    EXPECT_LT(order_manager_->get_cash(), 100000.0); // Should have spent money on the order (initial cash is 100000 from config)
}

TEST_F(LimitOrderFillTest, LimitOrderDoesNotFillWhenPriceNotCrossed) {
    // Submit a limit buy order for 100 shares at price 99.0
    auto order_id = order_manager_->submit_limit_order(
        "AAPL", qse::Order::Side::BUY, 100, 99.0, qse::Order::TimeInForce::GTC
    );
    
    // Tick with ask=100.0 (above limit price) - should not fill
    qse::Tick tick{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 100, 100, 100};
    order_manager_->process_tick(tick);
    
    auto order = order_manager_->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    EXPECT_EQ(order->filled_quantity, 0);
    
    // Verify portfolio was not updated
    EXPECT_EQ(order_manager_->get_position("AAPL"), 0);
    EXPECT_EQ(order_manager_->get_cash(), 100000.0); // Initial cash from config
}

TEST_F(LimitOrderFillTest, SellOrderPartialFills) {
    // First buy some shares to sell
    auto buy_order = order_manager_->submit_market_order("AAPL", qse::Order::Side::BUY, 200);
    qse::Tick buy_tick{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 200, 200, 200};
    order_manager_->process_tick(buy_tick);
    
    // Submit a limit sell order for 150 shares at price 100.2
    auto sell_order_id = order_manager_->submit_limit_order(
        "AAPL", qse::Order::Side::SELL, 150, 100.2, qse::Order::TimeInForce::GTC
    );
    
    // First tick: bid=100.2, bid_size=100 - should fill 100 shares
    qse::Tick tick1{"AAPL", qse::from_unix_ms(1001), 100.1, 100.2, 100.5, 100, 100, 100};
    order_manager_->process_tick(tick1);
    
    auto order = order_manager_->get_order(sell_order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_EQ(order->remaining_quantity(), 50);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.2);
    
    // Second tick: bid=100.2, bid_size=100 - should fill remaining 50 shares
    qse::Tick tick2{"AAPL", qse::from_unix_ms(1002), 100.1, 100.2, 100.5, 100, 100, 100};
    order_manager_->process_tick(tick2);
    
    order = order_manager_->get_order(sell_order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 150);
    EXPECT_EQ(order->remaining_quantity(), 0);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.2);
    
    // Verify portfolio was updated correctly (200 bought - 150 sold = 50 remaining)
    EXPECT_EQ(order_manager_->get_position("AAPL"), 50);
    EXPECT_GT(order_manager_->get_cash(), 10000.0); // Should have received money from the sell
}

TEST_F(LimitOrderFillTest, OrderBookLiquidityConsumption) {
    // Submit a limit buy order for 200 shares at price 100.0
    auto order_id = order_manager_->submit_limit_order(
        "AAPL", qse::Order::Side::BUY, 200, 100.0, qse::Order::TimeInForce::GTC
    );
    
    // Tick with ask=100.0, ask_size=150 - should fill 150 shares and consume all ask liquidity
    qse::Tick tick{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 100, 100, 150};
    order_manager_->process_tick(tick);
    
    auto order = order_manager_->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_EQ(order->remaining_quantity(), 100);
    
    // Verify the order book's ask size was consumed
    auto tob = order_book_->top_of_book("AAPL");
    EXPECT_EQ(tob.best_ask_size, 0); // All ask liquidity was consumed
    EXPECT_DOUBLE_EQ(tob.best_ask_price, 100.0); // Price should remain the same
} 