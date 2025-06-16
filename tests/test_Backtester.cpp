#include "Backtester.h"
#include "CSVDataReader.h"
#include "SMACrossoverStrategy.h"
#include "IOrderManager.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>

namespace qse {
namespace test {

class MockOrderManager : public IOrderManager {
public:
    MOCK_METHOD(void, execute_buy, (const Bar& bar), (override));
    MOCK_METHOD(void, execute_sell, (const Bar& bar), (override));
    MOCK_METHOD(int, get_position, (), (const, override));
    MOCK_METHOD(double, get_portfolio_value, (double current_price), (const, override));
    MOCK_METHOD(const std::vector<Trade>&, get_trade_log, (), (const, override));
};

class BacktesterTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories("test_data");
        file_path_ = "test_data/test_bars.csv";
        std::ofstream file(file_path_);
        file << "timestamp,open,high,low,close,volume\n";
        file << "1704067200,100.0,101.0,99.0,100.5,1000\n";
        file << "1704153600,100.5,102.0,100.0,101.5,2000\n";
        file << "1704240000,101.5,103.0,101.0,102.5,3000\n";
        order_manager_ = new MockOrderManager();
    }
    void TearDown() override {
        std::filesystem::remove(file_path_);
        delete order_manager_;
    }
    std::string file_path_;
    std::vector<Trade> empty_trade_log_;
    MockOrderManager* order_manager_ = nullptr;
};

TEST_F(BacktesterTest, CanCreateBacktester) {
    auto data_reader = std::make_unique<CSVDataReader>(file_path_);
    auto strategy = std::make_unique<SMACrossoverStrategy>(order_manager_, 2, 3);
    EXPECT_CALL(*order_manager_, get_trade_log()).WillRepeatedly(testing::ReturnRef(empty_trade_log_));
    Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::unique_ptr<IOrderManager>(order_manager_)
    );
    order_manager_ = nullptr; // Prevent double delete
    // This asserts that the code inside the parentheses runs to completion
    // without throwing a C++ exception.
    EXPECT_NO_THROW(backtester.run());
}

TEST_F(BacktesterTest, CanRunBacktest) {
    auto data_reader = std::make_unique<CSVDataReader>(file_path_);
    auto strategy = std::make_unique<SMACrossoverStrategy>(order_manager_, 2, 3);
    // Set up expectations
    EXPECT_CALL(*order_manager_, execute_buy(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(*order_manager_, execute_sell(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(*order_manager_, get_position()).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*order_manager_, get_portfolio_value(testing::_)).WillRepeatedly(testing::Return(100000.0));
    EXPECT_CALL(*order_manager_, get_trade_log()).WillRepeatedly(testing::ReturnRef(empty_trade_log_));
    Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::unique_ptr<IOrderManager>(order_manager_)
    );
    order_manager_ = nullptr; // Prevent double delete
    backtester.run();
}

} // namespace test
} // namespace qse 