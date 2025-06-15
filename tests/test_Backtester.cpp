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
    }
    void TearDown() override {
        std::filesystem::remove(file_path_);
    }
    std::string file_path_;
};

TEST_F(BacktesterTest, CanCreateBacktester) {
    auto data_reader = std::make_unique<CSVDataReader>(file_path_);
    auto order_manager = std::make_unique<MockOrderManager>();
    auto strategy = std::make_unique<SMACrossoverStrategy>(order_manager.get(), 2, 3);
    Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::unique_ptr<IOrderManager>(std::move(order_manager))
    );
    // This asserts that the code inside the parentheses runs to completion
    // without throwing a C++ exception.
    EXPECT_NO_THROW(backtester.run());
}

TEST_F(BacktesterTest, CanRunBacktest) {
    auto data_reader = std::make_unique<CSVDataReader>(file_path_);
    auto order_manager = std::make_unique<MockOrderManager>();
    auto strategy = std::make_unique<SMACrossoverStrategy>(order_manager.get(), 2, 3);
    // Set up expectations
    EXPECT_CALL(*order_manager, execute_buy(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(*order_manager, execute_sell(testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(*order_manager, get_position()).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*order_manager, get_portfolio_value(testing::_)).WillRepeatedly(testing::Return(100000.0));
    Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::unique_ptr<IOrderManager>(std::move(order_manager))
    );
    backtester.run();
}

} // namespace test
} // namespace qse 