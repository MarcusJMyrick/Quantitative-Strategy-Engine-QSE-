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

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

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

        sample_bars_.push_back({{std::chrono::system_clock::now()}, 100.0, 101.0, 99.0, 100.5, 1000});
        sample_bars_.push_back({{std::chrono::system_clock::now()}, 100.5, 102.0, 100.0, 101.5, 1200});
        sample_bars_.push_back({{std::chrono::system_clock::now()}, 101.5, 103.0, 101.0, 102.5, 1100});
    }
};

// --- THIS TEST IS NOW FIXED ---
TEST_F(BacktesterTest, CanCreateBacktesterAndRun) {
    // --- FIX: Define PRECISE expectations for what Backtester::run() calls ---
    EXPECT_CALL(*data_reader_, read_all_bars()).WillOnce(Return(std::ref(sample_bars_)));
    EXPECT_CALL(*strategy_, on_bar(_)).Times(sample_bars_.size());
    
    // Expect get_portfolio_value to be called for each bar + 1 final time.
    EXPECT_CALL(*order_manager_, get_portfolio_value(_))
        .Times(sample_bars_.size() + 1)
        .WillRepeatedly(Return(0.0));
    
    // Expect the final report calls once each.
    EXPECT_CALL(*order_manager_, get_trade_log()).WillOnce(ReturnRef(empty_trade_log_));
    EXPECT_CALL(*order_manager_, get_position()).WillOnce(Return(0));

    qse::Backtester backtester(
        "TEST_SYMBOL",
        std::unique_ptr<qse::IDataReader>(data_reader_),
        std::unique_ptr<qse::IStrategy>(strategy_),
        std::unique_ptr<qse::IOrderManager>(order_manager_)
    );
    
    EXPECT_NO_THROW(backtester.run());
}

TEST_F(BacktesterTest, CanRunBacktestWithRealComponents) {
    const std::string test_data_path = std::filesystem::current_path() / "test_data" / "test_data.csv";
    
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
