#include <gtest/gtest.h>
#include "qse/core/Backtester.h"
#include "qse/data/CSVDataReader.h"
#include "qse/order/OrderManager.h"
#include "qse/strategy/DoNothingStrategy.h"
#include <fstream>
#include <filesystem>

namespace qse {

class SmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a minimal test CSV file with just a few ticks
        create_test_csv();
    }

    void TearDown() override {
        // Clean up test file
        if (std::filesystem::exists(test_csv_file_)) {
            std::filesystem::remove(test_csv_file_);
        }
    }

    void create_test_csv() {
        std::ofstream file(test_csv_file_);
        file << "timestamp,price,volume\n";
        file << "1000,100.0,100\n";
        file << "1001,100.5,150\n";
        file << "1002,101.0,200\n";
        file << "1003,100.8,120\n";
        file << "1004,101.2,180\n";
        file << "1005,101.5,250\n";
        file << "1006,101.8,300\n";
        file << "1007,102.0,350\n";
        file << "1008,102.2,400\n";
        file << "1009,102.5,450\n";
        file.close();
    }

    const std::string test_csv_file_ = "smoke_test_ticks.csv";
};

// Test 1: Basic smoke test with do-nothing strategy
TEST_F(SmokeTest, DoNothingStrategySmokeTest) {
    // Create components
    auto data_reader = std::make_unique<CSVDataReader>(test_csv_file_);
    auto order_manager = std::make_unique<OrderManager>(10000.0, "smoke_equity.csv", "smoke_tradelog.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();

    // Create backtester with 1-minute bars
    Backtester backtester("TEST", std::move(data_reader), 
                         std::move(strategy), std::move(order_manager),
                         std::chrono::seconds(60));

    // This should not crash and should process all ticks
    EXPECT_NO_THROW(backtester.run());

    // Clean up output files
    std::filesystem::remove("smoke_equity.csv");
    std::filesystem::remove("smoke_tradelog.csv");
}

// Test 2: Verify strategy received expected data
TEST_F(SmokeTest, StrategyDataVerification) {
    // Create components
    auto data_reader = std::make_unique<CSVDataReader>(test_csv_file_);
    auto order_manager = std::make_unique<OrderManager>(10000.0, "smoke_equity.csv", "smoke_tradelog.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();

    // Get pointer before moving
    auto strategy_ptr = strategy.get();

    // Create backtester with 1-minute bars
    Backtester backtester("TEST", std::move(data_reader), 
                         std::move(strategy), std::move(order_manager),
                         std::chrono::seconds(60));

    // Run the backtest
    EXPECT_NO_THROW(backtester.run());

    // Verify strategy received data
    EXPECT_EQ(strategy_ptr->get_tick_count(), 10); // 10 ticks in our test file
    EXPECT_GT(strategy_ptr->get_bar_count(), 0);   // Should have at least one bar
    EXPECT_EQ(strategy_ptr->get_fill_count(), 0);  // No fills since we don't place orders

    // Clean up output files
    std::filesystem::remove("smoke_equity.csv");
    std::filesystem::remove("smoke_tradelog.csv");
}

// Test 3: Test with empty tick stream
TEST_F(SmokeTest, EmptyTickStreamSmokeTest) {
    // Create empty CSV file
    std::string empty_csv = "empty_smoke_test.csv";
    std::ofstream file(empty_csv);
    file << "timestamp,price,volume\n";
    file.close();

    // Create components
    auto data_reader = std::make_unique<CSVDataReader>(empty_csv);
    auto order_manager = std::make_unique<OrderManager>(10000.0, "smoke_equity.csv", "smoke_tradelog.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();

    auto strategy_ptr = strategy.get();

    // Create backtester
    Backtester backtester("TEST", std::move(data_reader), 
                         std::move(strategy), std::move(order_manager),
                         std::chrono::seconds(60));

    // This should not crash even with no data
    EXPECT_NO_THROW(backtester.run());

    // Verify strategy received no data
    EXPECT_EQ(strategy_ptr->get_tick_count(), 0);
    EXPECT_EQ(strategy_ptr->get_bar_count(), 0);
    EXPECT_EQ(strategy_ptr->get_fill_count(), 0);

    // Clean up
    std::filesystem::remove(empty_csv);
    std::filesystem::remove("smoke_equity.csv");
    std::filesystem::remove("smoke_tradelog.csv");
}

// Test 4: Test with very short bar interval
TEST_F(SmokeTest, ShortBarIntervalSmokeTest) {
    // Create components
    auto data_reader = std::make_unique<CSVDataReader>(test_csv_file_);
    auto order_manager = std::make_unique<OrderManager>(10000.0, "smoke_equity.csv", "smoke_tradelog.csv");
    auto strategy = std::make_unique<DoNothingStrategy>();

    auto strategy_ptr = strategy.get();

    // Create backtester with 1-second bars (very short)
    Backtester backtester("TEST", std::move(data_reader), 
                         std::move(strategy), std::move(order_manager),
                         std::chrono::seconds(1));

    // This should not crash
    EXPECT_NO_THROW(backtester.run());

    // Verify strategy received data
    EXPECT_EQ(strategy_ptr->get_tick_count(), 10);
    EXPECT_GT(strategy_ptr->get_bar_count(), 0);

    // Clean up output files
    std::filesystem::remove("smoke_equity.csv");
    std::filesystem::remove("smoke_tradelog.csv");
}

} // namespace qse 