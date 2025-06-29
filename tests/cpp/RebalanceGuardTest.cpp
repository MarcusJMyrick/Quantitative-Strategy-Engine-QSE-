#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "qse/strategy/FactorStrategy.h"
#include "qse/order/IOrderManager.h"
#include "qse/data/Data.h"
#include "mocks/MockOrderManager.h"
#include <memory>
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace qse;
using ::testing::Return;
using ::testing::_;
using ::testing::StrictMock;

class FactorStrategyRebalanceGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        order_manager = std::make_shared<StrictMock<MockOrderManager>>();
    }
    std::shared_ptr<StrictMock<MockOrderManager>> order_manager;
};

TEST_F(FactorStrategyRebalanceGuardTest, NoDuplicateOrders) {
    // Same weights loaded twice in same session
    // Expect: execute_* called once total (use Times(1))
    
    // Create test weights file
    std::string test_dir = "test_weights_no_duplicate";
    std::filesystem::create_directories(test_dir);
    std::ofstream file(test_dir + "/weights_20241215.csv");
    file << "symbol,weight\nAAPL,0.05\nMSFT,-0.05\n";
    file.close();
    
    // Create strategy with mock order manager
    auto strategy = std::make_unique<FactorStrategy>(
        order_manager, "TEST", test_dir, 1.0);
    
    // Create timestamp for 2024-12-15
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11; // December
    tm.tm_mday = 15;
    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // Create close prices map
    std::unordered_map<std::string, double> close_prices = {
        {"AAPL", 100.0},
        {"MSFT", 100.0}
    };
    
    // Set up mock expectations for FIRST call only
    EXPECT_CALL(*order_manager, get_cash()).WillOnce(Return(1000000.0)); // $1M cash
    EXPECT_CALL(*order_manager, get_position("AAPL")).WillOnce(Return(0)); // No position
    EXPECT_CALL(*order_manager, get_position("MSFT")).WillOnce(Return(0)); // No position
    
    // Expected orders: AAPL buy and MSFT sell (should only happen once)
    EXPECT_CALL(*order_manager, execute_buy("AAPL", 500, 100.0)).Times(1);
    EXPECT_CALL(*order_manager, execute_sell("MSFT", 500, 100.0)).Times(1);
    
    // First call - should execute orders
    strategy->on_day_close_with_prices(timestamp, close_prices);
    
    // Second call with same timestamp - should skip rebalancing
    strategy->on_day_close_with_prices(timestamp, close_prices);
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
}

TEST_F(FactorStrategyRebalanceGuardTest, MinDollarThreshold) {
    // Δw produces <$50 change
    // Expect: No orders issued (Times(0))
    
    // Create test weights file with very small weights
    std::string test_dir = "test_weights_min_threshold";
    std::filesystem::create_directories(test_dir);
    std::ofstream file(test_dir + "/weights_20241215.csv");
    file << "symbol,weight\nAAPL,0.00001\n"; // 0.001% weight = $10 on $1M NAV
    file.close();
    
    // Create strategy with high min dollar threshold
    auto strategy = std::make_unique<FactorStrategy>(
        order_manager, "TEST", test_dir, 50.0); // $50 minimum threshold
    
    // Create timestamp for 2024-12-15
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11; // December
    tm.tm_mday = 15;
    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // Create close prices map
    std::unordered_map<std::string, double> close_prices = {
        {"AAPL", 100.0}
    };
    
    // Set up mock expectations
    EXPECT_CALL(*order_manager, get_cash()).WillOnce(Return(1000000.0)); // $1M cash
    EXPECT_CALL(*order_manager, get_position("AAPL")).WillOnce(Return(0)); // No position
    
    // Expected: NO orders should be issued (Times(0))
    EXPECT_CALL(*order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*order_manager, execute_sell(_, _, _)).Times(0);
    
    // Call on_day_close_with_prices - should skip due to min threshold
    strategy->on_day_close_with_prices(timestamp, close_prices);
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
}
