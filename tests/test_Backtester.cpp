#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Backtester.h"

#include "IDataReader.h"
#include "IStrategy.h"
#include "IOrderManager.h"

#include "mocks/MockDataReader.h"
#include "mocks/MockStrategy.h"
#include "mocks/MockOrderManager.h"
#include "CSVDataReader.h" 
#include "SMACrossoverStrategy.h"
#include "OrderManager.h"
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
    std::vector<qse::Bar> sample_bars_;
    const std::vector<qse::Trade> empty_trade_log_{};

    void SetUp() override {
        data_reader_ = new MockDataReader();
        strategy_ = new MockStrategy();
        order_manager_ = new MockOrderManager();

        // Create sample bars with symbol field
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

// This test should already be passing.
TEST_F(BacktesterTest, CanCreateBacktesterAndRun) {
    EXPECT_CALL(*data_reader_, read_all_bars()).WillOnce(Return(std::ref(sample_bars_)));
    EXPECT_CALL(*strategy_, on_bar(_)).Times(sample_bars_.size());
    EXPECT_CALL(*order_manager_, get_portfolio_value(_)).WillRepeatedly(Return(0.0));
    EXPECT_CALL(*order_manager_, get_trade_log()).WillRepeatedly(ReturnRef(empty_trade_log_));
    EXPECT_CALL(*order_manager_, get_position()).WillRepeatedly(Return(0));

    qse::Backtester backtester(
        "TEST_SYMBOL",
        std::unique_ptr<qse::IDataReader>(data_reader_),
        std::unique_ptr<qse::IStrategy>(strategy_),
        std::unique_ptr<qse::IOrderManager>(order_manager_)
    );
    
    EXPECT_NO_THROW(backtester.run());
}

TEST_F(BacktesterTest, CanRunBacktestWithRealComponents) {
    // Use a simple relative path from the build directory
    const std::string test_data_path = "../test_data/test_data.csv";
    
    auto real_data_reader = std::make_unique<qse::CSVDataReader>(test_data_path);
    auto real_order_manager = std::make_unique<qse::OrderManager>(100000.0, 1.0, 0.01);
    auto real_strategy = std::make_unique<qse::SMACrossoverStrategy>(real_order_manager.get(), 10, 20);

    qse::Backtester backtester(
        "TEST_REAL",
        std::move(real_data_reader),
        std::move(real_strategy),
        std::move(real_order_manager)
    );

    EXPECT_NO_THROW(backtester.run());
}
