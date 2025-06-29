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

class DeltaOrderTest : public ::testing::Test {
protected:
    void SetUp() override {
        order_manager = std::make_shared<StrictMock<MockOrderManager>>();
    }
    std::shared_ptr<StrictMock<MockOrderManager>> order_manager;
};

TEST_F(DeltaOrderTest, CashNeutral) {
    // Target weights: +5% AAPL, -5% MSFT
    // Current positions: 0
    // Expect: Σ$ longs ≈ Σ$ shorts within 1e-3
    
    // Create test weights file
    std::string test_dir = "test_weights_cash_neutral";
    std::filesystem::create_directories(test_dir);
    std::ofstream file(test_dir + "/weights_20241215.csv");
    file << "symbol,weight\nAAPL,0.05\nMSFT,-0.05\n";
    file.close();
    
    // Create strategy with mock order manager
    auto strategy = std::make_unique<FactorStrategy>(
        order_manager, "TEST", test_dir, 1.0);
    
    // Set up mock expectations
    // NAV calculation: cash + positions
    EXPECT_CALL(*order_manager, get_cash()).WillOnce(Return(1000000.0)); // $1M cash
    EXPECT_CALL(*order_manager, get_position("AAPL")).WillOnce(Return(0)); // No position
    EXPECT_CALL(*order_manager, get_position("MSFT")).WillOnce(Return(0)); // No position
    
    // Expected orders: AAPL buy and MSFT sell
    // NAV = $1M, AAPL target = +5% = +$50K, MSFT target = -5% = -$50K
    // At $100/share: AAPL buy 500 shares, MSFT sell 500 shares
    EXPECT_CALL(*order_manager, execute_buy("AAPL", 500, 100.0)).Times(1);
    EXPECT_CALL(*order_manager, execute_sell("MSFT", 500, 100.0)).Times(1);
    
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
    
    // Call on_day_close_with_prices which should trigger the delta orders
    strategy->on_day_close_with_prices(timestamp, close_prices);
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
}

TEST_F(DeltaOrderTest, QuantityRounding) {
    // Price: 123.45, target Δw = $1,000
    // Expect: Share count rounds down to nearest lot (e.g., 8 shares for 1000/123.45)
    
    // Create test weights file
    std::string test_dir = "test_weights_rounding";
    std::filesystem::create_directories(test_dir);
    std::ofstream file(test_dir + "/weights_20241215.csv");
    file << "symbol,weight\nAAPL,0.001\n"; // 0.1% weight
    file.close();
    
    // Create strategy with mock order manager
    auto strategy = std::make_unique<FactorStrategy>(
        order_manager, "TEST", test_dir, 1.0);
    
    // Set up mock expectations
    // NAV calculation: $1M cash, no positions
    EXPECT_CALL(*order_manager, get_cash()).WillOnce(Return(1000000.0));
    EXPECT_CALL(*order_manager, get_position("AAPL")).WillOnce(Return(0));
    
    // Expected order: AAPL buy with rounding
    // NAV = $1M, AAPL target = +0.1% = +$1K
    // At $123.45/share: $1000 / $123.45 = 8.10 shares, rounds down to 8 shares
    EXPECT_CALL(*order_manager, execute_buy("AAPL", 8, 123.45)).Times(1);
    
    // Create timestamp for 2024-12-15
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11; // December
    tm.tm_mday = 15;
    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // Create close prices map
    std::unordered_map<std::string, double> close_prices = {
        {"AAPL", 123.45}
    };
    
    // Call on_day_close_with_prices which should trigger the delta orders
    strategy->on_day_close_with_prices(timestamp, close_prices);
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
}
