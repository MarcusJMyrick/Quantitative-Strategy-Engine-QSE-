#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "qse/core/Backtester.h"

#include "qse/data/IDataReader.h"
#include "qse/strategy/IStrategy.h"
#include "qse/order/IOrderManager.h"

#include "mocks/MockDataReader.h"
#include "mocks/MockStrategy.h"
#include "mocks/MockOrderManager.h"
#include "qse/data/CSVDataReader.h" 
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"
#include <filesystem>

#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

// Bring necessary GMock actions into scope
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::StrictMock;
using ::testing::AtLeast;

// This helper macro correctly converts the CMake preprocessor definition into a C++ string literal.
#define QSE_XSTR(s) #s
#define QSE_STR(s) QSE_XSTR(s)

using namespace qse;

// Add operator== for qse::Tick for GoogleMock
namespace qse {
    inline bool operator==(const Tick& lhs, const Tick& rhs) {
        return lhs.timestamp == rhs.timestamp && lhs.price == rhs.price && lhs.volume == rhs.volume;
    }
}

class BacktesterTest : public ::testing::Test {
protected:
    MockDataReader* data_reader_;
    MockStrategy* strategy_;
    MockOrderManager* order_manager_;
    std::vector<qse::Tick> sample_ticks_;
    std::vector<qse::Bar> sample_bars_;
    const std::vector<qse::Trade> empty_trade_log_{};

    void SetUp() override {
        data_reader_ = new MockDataReader();
        strategy_ = new MockStrategy();
        order_manager_ = new MockOrderManager();

        // Create sample ticks for the primary test
        qse::Tick tick1;
        tick1.timestamp = std::chrono::system_clock::now();
        tick1.price = 100.0;
        tick1.volume = 1000;
        sample_ticks_.push_back(tick1);

        qse::Tick tick2;
        tick2.timestamp = std::chrono::system_clock::now() + std::chrono::seconds(1);
        tick2.price = 101.0;
        tick2.volume = 1200;
        sample_ticks_.push_back(tick2);

        qse::Tick tick3;
        tick3.timestamp = std::chrono::system_clock::now() + std::chrono::seconds(2);
        tick3.price = 102.0;
        tick3.volume = 1100;
        sample_ticks_.push_back(tick3);

        // Create sample bars for other tests if needed
        qse::Bar bar1;
        bar1.symbol = "TEST";
        bar1.timestamp = std::chrono::system_clock::now();
        bar1.open = 100.0;
        bar1.high = 101.0;
        bar1.low = 99.0;
        bar1.close = 100.5;
        bar1.volume = 1000;
        sample_bars_.push_back(bar1);

        qse::Bar bar2;
        bar2.symbol = "TEST";
        bar2.timestamp = std::chrono::system_clock::now();
        bar2.open = 100.5;
        bar2.high = 102.0;
        bar2.low = 100.0;
        bar2.close = 101.5;
        bar2.volume = 1200;
        sample_bars_.push_back(bar2);

        qse::Bar bar3;
        bar3.symbol = "TEST";
        bar3.timestamp = std::chrono::system_clock::now();
        bar3.open = 101.5;
        bar3.high = 103.0;
        bar3.low = 101.0;
        bar3.close = 102.5;
        bar3.volume = 1100;
        sample_bars_.push_back(bar3);
    }
};

// Updated test for the new tick-driven workflow
TEST_F(BacktesterTest, CanCreateBacktesterAndRun) {
    // Setup Expectations for a tick-driven workflow
    // Expect read_all_ticks() to be called once and return our sample ticks.
    EXPECT_CALL(*data_reader_, read_all_ticks()).WillOnce(ReturnRef(sample_ticks_));
    
    // Expect on_tick() to be called for each tick we provided.
    EXPECT_CALL(*strategy_, on_tick(_)).Times(sample_ticks_.size());

    // We no longer expect read_all_bars or on_bar to be called in this test.

    qse::Backtester backtester(
        "TEST_SYMBOL",
        std::unique_ptr<qse::IDataReader>(data_reader_),
        std::unique_ptr<qse::IStrategy>(strategy_),
        std::unique_ptr<qse::IOrderManager>(order_manager_)
    );
    
    EXPECT_NO_THROW(backtester.run());
}

TEST_F(BacktesterTest, CanRunBacktestWithRealComponents) {
    auto data_reader = std::make_unique<qse::CSVDataReader>("../test_data/test_data.csv");
    auto real_order_manager = std::make_unique<qse::OrderManager>(100000.0, "test_equity.csv", "test_tradelog.csv");

    // FIX: Add a bar_duration (e.g., 24 hours) as the fourth argument
    auto real_strategy = std::make_unique<qse::SMACrossoverStrategy>(
        real_order_manager.get(), 10, 20, std::chrono::hours(24)
    );

    qse::Backtester backtester(
        "TEST_SYMBOL",
        std::move(data_reader),
        std::move(real_strategy),
        std::move(real_order_manager)
    );

    // Run the backtest
    backtester.run();

    // Verify that output files were created
    EXPECT_TRUE(std::filesystem::exists("test_equity.csv"));
    EXPECT_TRUE(std::filesystem::exists("test_tradelog.csv"));
    
    // Clean up test files
    std::filesystem::remove("test_equity.csv");
    std::filesystem::remove("test_tradelog.csv");
}

class BacktesterTickIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test tick data
        test_ticks_ = {
            {qse::from_unix_ms(1000), 100.0, 100},
            {qse::from_unix_ms(1001), 100.5, 150},
            {qse::from_unix_ms(1002), 101.0, 200},
            {qse::from_unix_ms(1003), 100.8, 120},
            {qse::from_unix_ms(1004), 101.2, 180},
            {qse::from_unix_ms(1005), 101.5, 250},
            {qse::from_unix_ms(1006), 101.8, 300},
            {qse::from_unix_ms(1007), 102.0, 350},
            {qse::from_unix_ms(1008), 102.2, 400},
            {qse::from_unix_ms(1009), 102.5, 450}
        };
        empty_ticks_.clear();
    }

    std::vector<qse::Tick> test_ticks_;
    std::vector<qse::Tick> empty_ticks_;
};

// Test 1: Basic tick stream integration with mocks
TEST_F(BacktesterTickIntegrationTest, TickStreamIntegration) {
    // Create mocks
    auto mock_data_reader = std::make_unique<StrictMock<MockDataReader>>();
    auto mock_strategy = std::make_unique<StrictMock<MockStrategy>>();
    auto mock_order_manager = std::make_unique<StrictMock<MockOrderManager>>();

    // Set expectations
    EXPECT_CALL(*mock_data_reader, read_all_ticks())
        .WillOnce(ReturnRef(test_ticks_));

    // Expect strategy to receive each tick
    for (const auto& tick : test_ticks_) {
        EXPECT_CALL(*mock_strategy, on_tick(tick))
            .Times(1);
    }

    // Create and run backtester
    qse::Backtester backtester("TEST", std::move(mock_data_reader), 
                         std::move(mock_strategy), std::move(mock_order_manager));
    
    // This should not throw and should process all ticks
    EXPECT_NO_THROW(backtester.run());
}

// Test 2: Empty tick stream handling
TEST_F(BacktesterTickIntegrationTest, EmptyTickStream) {
    auto mock_data_reader = std::make_unique<StrictMock<MockDataReader>>();
    auto mock_strategy = std::make_unique<StrictMock<MockStrategy>>();
    auto mock_order_manager = std::make_unique<StrictMock<MockOrderManager>>();

    // Return empty tick vector
    EXPECT_CALL(*mock_data_reader, read_all_ticks())
        .WillOnce(ReturnRef(empty_ticks_));

    // Strategy should not be called with any ticks
    EXPECT_CALL(*mock_strategy, on_tick(_))
        .Times(0);

    qse::Backtester backtester("TEST", std::move(mock_data_reader), 
                         std::move(mock_strategy), std::move(mock_order_manager));
    
    EXPECT_NO_THROW(backtester.run());
}

// Test 3: Real CSV data reader integration
TEST_F(BacktesterTickIntegrationTest, RealCSVDataReaderIntegration) {
    // Create a simple test CSV file with tick data
    std::string test_csv_content = 
        "timestamp,price,volume\n"
        "1000,100.0,100\n"
        "1001,100.5,150\n"
        "1002,101.0,200\n"
        "1003,100.8,120\n"
        "1004,101.2,180\n";

    std::string test_file = "test_ticks.csv";
    std::ofstream file(test_file);
    file << test_csv_content;
    file.close();

    // Create real components
    auto data_reader = std::make_unique<qse::CSVDataReader>(test_file);
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    auto strategy = std::make_unique<qse::SMACrossoverStrategy>(order_manager.get(), 5, 10, std::chrono::minutes(1));

    // Create and run backtester
    qse::Backtester backtester("TEST", std::move(data_reader), 
                         std::move(strategy), std::move(order_manager));
    
    EXPECT_NO_THROW(backtester.run());

    // Clean up
    std::remove(test_file.c_str());
}

// Test 4: Tick processing order verification
TEST_F(BacktesterTickIntegrationTest, TickProcessingOrder) {
    auto mock_data_reader = std::make_unique<StrictMock<MockDataReader>>();
    auto mock_strategy = std::make_unique<StrictMock<MockStrategy>>();
    auto mock_order_manager = std::make_unique<StrictMock<MockOrderManager>>();

    // Create a sequence of expectations to verify order
    ::testing::InSequence seq;
    
    EXPECT_CALL(*mock_data_reader, read_all_ticks())
        .WillOnce(ReturnRef(test_ticks_));

    // Expect ticks to be processed in order
    for (const auto& tick : test_ticks_) {
        EXPECT_CALL(*mock_strategy, on_tick(tick))
            .Times(1);
    }

    qse::Backtester backtester("TEST", std::move(mock_data_reader), 
                         std::move(mock_strategy), std::move(mock_order_manager));
    
    EXPECT_NO_THROW(backtester.run());
}

// Test 5: Large tick stream performance
TEST_F(BacktesterTickIntegrationTest, LargeTickStream) {
    // Create a larger dataset
    std::vector<qse::Tick> large_tick_stream;
    for (int i = 0; i < 1000; ++i) {
        large_tick_stream.push_back({
            qse::from_unix_ms(1000 + i),
            100.0 + (i * 0.01),
            99.5 + (i * 0.01),  // bid
            100.5 + (i * 0.01), // ask
            static_cast<qse::Volume>(100 + i)
        });
    }

    auto mock_data_reader = std::make_unique<StrictMock<MockDataReader>>();
    auto mock_strategy = std::make_unique<StrictMock<MockStrategy>>();
    auto mock_order_manager = std::make_unique<StrictMock<MockOrderManager>>();

    EXPECT_CALL(*mock_data_reader, read_all_ticks())
        .WillOnce(ReturnRef(large_tick_stream));

    // Expect all ticks to be processed
    EXPECT_CALL(*mock_strategy, on_tick(_))
        .Times(1000);

    qse::Backtester backtester("TEST", std::move(mock_data_reader), 
                         std::move(mock_strategy), std::move(mock_order_manager));
    
    EXPECT_NO_THROW(backtester.run());
}

// Test 6: Error handling in tick processing
TEST_F(BacktesterTickIntegrationTest, TickProcessingErrorHandling) {
    auto mock_data_reader = std::make_unique<StrictMock<MockDataReader>>();
    auto mock_strategy = std::make_unique<StrictMock<MockStrategy>>();
    auto mock_order_manager = std::make_unique<StrictMock<MockOrderManager>>();

    EXPECT_CALL(*mock_data_reader, read_all_ticks())
        .WillOnce(ReturnRef(test_ticks_));

    // Make strategy throw on first tick
    EXPECT_CALL(*mock_strategy, on_tick(test_ticks_[0]))
        .WillOnce(::testing::Throw(std::runtime_error("Strategy error")));

    qse::Backtester backtester("TEST", std::move(mock_data_reader), 
                         std::move(mock_strategy), std::move(mock_order_manager));
    
    // The backtester should handle the error gracefully
    // Note: Current implementation doesn't have error handling, so this will throw
    // This test documents the current behavior
    EXPECT_THROW(backtester.run(), std::runtime_error);
}
