#include <gtest/gtest.h>
#include "qse/core/Config.h"
#include "qse/order/OrderManager.h"
#include "qse/data/Data.h"
#include <fstream>

class SlippageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test config file with slippage coefficients
        test_config_content_ = R"(
symbols:
  TEST:
    slippage:
      linear_coeff: 0.001
  AAPL:
    slippage:
      linear_coeff: 0.0005
  GOOGL:
    slippage:
      linear_coeff: 0.0008

backtester:
  initial_cash: 100000.0
  commission_rate: 0.001
  min_trade_size: 1

data:
  base_path: "./data"
  processed_path: "./data/processed"
  results_path: "./results"
)";
        
        // Write test config to file
        std::ofstream config_file("test_slippage_config.yaml");
        config_file << test_config_content_;
        config_file.close();
        
        // Load the config
        config_.load_config("test_slippage_config.yaml");
    }
    
    void TearDown() override {
        // Clean up test files
        std::remove("test_slippage_config.yaml");
        std::remove("test_equity.csv");
        std::remove("test_tradelog.csv");
    }
    
    qse::Config config_;
    std::string test_config_content_;
};

// Test 1: Config loads slippage coefficients correctly
TEST_F(SlippageTest, ConfigLoadsSlippageCoefficients) {
    EXPECT_DOUBLE_EQ(config_.get_slippage_coeff("TEST"), 0.001);
    EXPECT_DOUBLE_EQ(config_.get_slippage_coeff("AAPL"), 0.0005);
    EXPECT_DOUBLE_EQ(config_.get_slippage_coeff("GOOGL"), 0.0008);
    EXPECT_DOUBLE_EQ(config_.get_slippage_coeff("UNKNOWN"), 0.0); // Default for unknown symbols
}

// Test 2: Market buy order with slippage
TEST_F(SlippageTest, MarketBuyOrderWithSlippage) {
    auto order_manager = std::make_unique<qse::OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a market buy order for TEST symbol
    auto order_id = order_manager->submit_market_order("TEST", qse::Order::Side::BUY, 100);
    
    // Create a tick with mid price 50.0
    qse::Tick test_tick{"TEST", qse::from_unix_ms(1000), 50.0, 49.5, 50.5, 100};
    
    // Process the tick - should fill with slippage
    order_manager->process_tick(test_tick);
    
    // Verify order is filled
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    
    // Expected fill price: 50.0 + (50.0 * 0.001 * 100) = 50.0 + 5.0 = 55.0
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 55.0);
    
    // Verify cash is reduced by the slippage-adjusted price
    EXPECT_DOUBLE_EQ(order_manager->get_cash(), 100000.0 - (100 * 55.0));
}

// Test 3: Market sell order with slippage
TEST_F(SlippageTest, MarketSellOrderWithSlippage) {
    auto order_manager = std::make_unique<qse::OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // First buy some shares
    order_manager->submit_market_order("TEST", qse::Order::Side::BUY, 100);
    qse::Tick buy_tick{"TEST", qse::from_unix_ms(1000), 50.0, 49.5, 50.5, 100};
    order_manager->process_tick(buy_tick);
    
    // Submit a market sell order
    auto order_id = order_manager->submit_market_order("TEST", qse::Order::Side::SELL, 50);
    
    // Create a tick with mid price 60.0
    qse::Tick sell_tick{"TEST", qse::from_unix_ms(1001), 60.0, 59.5, 60.5, 100};
    
    // Process the tick - should fill with slippage
    order_manager->process_tick(sell_tick);
    
    // Verify order is filled
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 50);
    
    // Expected fill price: 60.0 - (60.0 * 0.001 * 50) = 60.0 - 3.0 = 57.0
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 57.0);
}

// Test 4: No slippage for symbols without config
TEST_F(SlippageTest, NoSlippageForUnknownSymbol) {
    auto order_manager = std::make_unique<qse::OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a market buy order for unknown symbol
    auto order_id = order_manager->submit_market_order("UNKNOWN", qse::Order::Side::BUY, 100);
    
    // Create a tick with mid price 50.0
    qse::Tick test_tick{"UNKNOWN", qse::from_unix_ms(1000), 50.0, 49.5, 50.5, 100};
    
    // Process the tick - should fill without slippage
    order_manager->process_tick(test_tick);
    
    // Verify order is filled
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    
    // Expected fill price: 50.0 (no slippage)
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 50.0);
}

// Test 5: Limit order with slippage
TEST_F(SlippageTest, LimitOrderWithSlippage) {
    auto order_manager = std::make_unique<qse::OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a limit buy order at 50.0
    auto order_id = order_manager->submit_limit_order("TEST", qse::Order::Side::BUY, 100, 50.0, qse::Order::TimeInForce::GTC);
    
    // Create a tick that crosses the limit (ask <= limit price)
    qse::Tick test_tick{"TEST", qse::from_unix_ms(1000), 50.0, 49.5, 50.0, 100};
    
    // Process the tick - should fill at limit price with slippage
    order_manager->process_tick(test_tick);
    
    // Verify order is filled
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    
    // Expected fill price: 50.0 + (50.0 * 0.001 * 100) = 50.0 + 5.0 = 55.0
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 55.0);
} 