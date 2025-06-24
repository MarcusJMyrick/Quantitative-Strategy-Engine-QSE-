#include <gtest/gtest.h>
#include "qse/core/Backtester.h"
#include "qse/strategy/DoNothingStrategy.h"
#include "qse/data/CSVDataReader.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Config.h"
#include <memory>
#include <filesystem>

namespace qse {

class TickDrivenArchitectureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple config file for testing
        std::ofstream config_file("test_config.yaml");
        config_file << "slippage:\n";
        config_file << "  TEST: 0.001\n";
        config_file << "  AAPL: 0.0005\n";
        config_file << "  GOOGL: 0.0008\n";
        config_file << "backtester:\n";
        config_file << "  initial_cash: 100000.0\n";
        config_file << "  commission_rate: 0.001\n";
        config_file << "  min_trade_size: 1\n";
        config_file.close();
        
        // Load the config
        config_.load_config("test_config.yaml");
        
        // Create test files if they don't exist
        std::ofstream equity_file("test_equity.csv");
        std::ofstream tradelog_file("test_tradelog.csv");
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove("test_equity.csv");
        std::filesystem::remove("test_tradelog.csv");
        std::filesystem::remove("test_config.yaml");
        std::filesystem::remove("temp_test_ticks.csv");
    }
    
    Config config_;
};

// Smoke test: verify that the tick-driven architecture doesn't crash
TEST_F(TickDrivenArchitectureTest, DoNothingStrategySmokeTest) {
    // Create test tick data
    std::ofstream tick_file("temp_test_ticks.csv");
    tick_file << "timestamp,symbol,price,volume,bid,ask,bid_size,ask_size\n";
    tick_file << "1000000,TEST,100.0,100,99.5,100.5,100,100\n";
    tick_file << "1001000,TEST,100.5,150,100.0,101.0,150,150\n";
    tick_file << "1002000,TEST,101.0,200,100.5,101.5,200,200\n";
    tick_file.close();
    
    // Create components for the backtester
    auto data_reader = std::make_unique<CSVDataReader>("temp_test_ticks.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();
    auto order_manager = std::make_unique<OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // Create the backtester with the new tick-driven architecture
    Backtester backtester(
        "TEST",
        std::move(data_reader),
        std::move(strategy),
        std::move(order_manager),
        std::chrono::seconds(60) // 1-minute bars
    );
    
    // This should not crash
    EXPECT_NO_THROW(backtester.run());
}

// Test that the architecture processes ticks correctly
TEST_F(TickDrivenArchitectureTest, ProcessesTicksWithoutCrash) {
    // Create a minimal tick CSV for testing
    std::ofstream tick_file("temp_test_ticks.csv");
    tick_file << "timestamp,symbol,price,volume,bid,ask,bid_size,ask_size\n";
    tick_file << "1000000,TEST,100.0,100,99.5,100.5,100,100\n";
    tick_file << "1001000,TEST,100.5,150,100.0,101.0,150,150\n";
    tick_file << "1002000,TEST,101.0,200,100.5,101.5,200,200\n";
    tick_file.close();
    
    // Create components for the backtester
    auto data_reader = std::make_unique<CSVDataReader>("temp_test_ticks.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();
    auto order_manager = std::make_unique<OrderManager>(config_, "test_equity.csv", "test_tradelog.csv");
    
    // Create the backtester
    Backtester backtester(
        "TEST",
        std::move(data_reader),
        std::move(strategy),
        std::move(order_manager),
        std::chrono::seconds(60)
    );
    
    // This should process the ticks without crashing
    EXPECT_NO_THROW(backtester.run());
}

} // namespace qse 