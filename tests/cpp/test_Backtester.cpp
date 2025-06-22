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

// Bring necessary GMock actions into scope
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

// This helper macro correctly converts the CMake preprocessor definition into a C++ string literal.
#define QSE_XSTR(s) #s
#define QSE_STR(s) QSE_XSTR(s)

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
