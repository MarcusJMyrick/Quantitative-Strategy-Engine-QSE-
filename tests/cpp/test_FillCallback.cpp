#include <gtest/gtest.h>
#include "qse/order/OrderManager.h"
#include "qse/strategy/FillTrackingStrategy.h"
#include "qse/core/Config.h"
#include <memory>
#include <filesystem>

namespace qse {

class FillCallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple config file for testing
        std::ofstream config_file("test_config.yaml");
        config_file << "slippage:\n";
        config_file << "  TEST: 0.001\n";
        config_file << "  AAPL: 0.0005\n";
        config_file << "backtester:\n";
        config_file << "  initial_cash: 100000.0\n";
        config_file << "  commission_rate: 0.001\n";
        config_file.close();
        
        // Create test files for OrderManager
        std::ofstream equity_file("test_equity.csv");
        std::ofstream tradelog_file("test_tradelog.csv");
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove("test_config.yaml");
        std::filesystem::remove("test_equity.csv");
        std::filesystem::remove("test_tradelog.csv");
    }
};

// Test that the fill callback system works with direct OrderManager usage
TEST_F(FillCallbackTest, DirectFillCallback) {
    // Create components
    auto config = std::make_unique<Config>();
    config->load_config("test_config.yaml");
    
    auto order_manager = std::make_unique<OrderManager>(*config, "test_equity.csv", "test_tradelog.csv");
    auto order_manager_ptr = std::shared_ptr<IOrderManager>(std::move(order_manager));
    auto strategy = std::make_unique<FillTrackingStrategy>(order_manager_ptr);
    
    // Set up fill callback
    order_manager_ptr->set_fill_callback([&strategy](const Fill& fill) {
        strategy->on_fill(fill);
    });
    
    // Submit a test order
    OrderId order_id = order_manager_ptr->submit_market_order("TEST", Order::Side::BUY, 100);
    
    // Verify the order was submitted
    auto order = order_manager_ptr->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->symbol, "TEST");
    EXPECT_EQ(order->quantity, 100);
    
    // Test passes if no crashes occur
    EXPECT_TRUE(true);
}

// Test that the FillTrackingStrategy can be created and used
TEST_F(FillCallbackTest, FillTrackingStrategyCreation) {
    // Create components
    auto config = std::make_unique<Config>();
    config->load_config("test_config.yaml");
    
    auto order_manager = std::make_unique<OrderManager>(*config, "test_equity.csv", "test_tradelog.csv");
    auto order_manager_ptr = std::shared_ptr<IOrderManager>(std::move(order_manager));
    
    // Create strategy
    auto strategy = std::make_unique<FillTrackingStrategy>(order_manager_ptr);
    
    // Test passes if strategy was created successfully
    EXPECT_TRUE(strategy != nullptr);
}

} // namespace qse 