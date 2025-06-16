// In tests/test_Strategy.cpp

#include <gtest/gtest.h>
#include <gmock/gmock.h> // The mocking library header
#include "IOrderManager.h"
#include "SMACrossoverStrategy.h"
#include "Data.h"
#include <vector>

// This Mock Order Manager now perfectly matches the IOrderManager interface.
class MockOrderManager : public qse::IOrderManager {
public:
    // MOCK_METHOD signature now correctly uses 'const qse::Bar&'
    MOCK_METHOD(void, execute_buy, (const qse::Bar& bar), (override));
    MOCK_METHOD(void, execute_sell, (const qse::Bar& bar), (override));
    
    // You must also mock the other pure virtual functions from the interface
    MOCK_METHOD(int, get_position, (), (const, override));
    MOCK_METHOD(double, get_portfolio_value, (double current_price), (const, override));
    MOCK_METHOD(const std::vector<qse::Trade>&, get_trade_log, (), (const, override));
};

// Test fixture to hold our mock object
class SMACrossoverStrategyTest : public ::testing::Test {
protected:
    MockOrderManager mock_order_manager;
};

TEST_F(SMACrossoverStrategyTest, GeneratesBuySignalOnGoldenCross) {
    // 1. Arrange: Create the strategy with a short window of 3 and long of 5
    qse::SMACrossoverStrategy strategy(&mock_order_manager, 3, 5);

    // This specific price sequence will cause a crossover on the last bar.
    std::vector<double> prices = {100, 99, 98, 97, 96, 105, 106, 107};
    
    // 2. Expectation: We expect the execute_buy method to be called exactly once.
    // The `::testing::_` wildcard correctly matches the Bar object argument.
    EXPECT_CALL(mock_order_manager, execute_buy(::testing::_)).Times(1);
    EXPECT_CALL(mock_order_manager, execute_sell(::testing::_)).Times(0);

    // 3. Act: Feed the prices to the strategy one by one
    for (double price : prices) {
        qse::Bar bar;
        bar.close = price;
        strategy.on_bar(bar);
    }
}