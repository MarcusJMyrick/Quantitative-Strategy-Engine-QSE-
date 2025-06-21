// In tests/test_Strategy.cpp

#include <gtest/gtest.h>
#include <gmock/gmock.h> // The mocking library header
#include "qse/order/IOrderManager.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/data/Data.h"
#include <vector>

// This Mock Order Manager now perfectly matches the IOrderManager interface.
class MockOrderManager : public qse::IOrderManager {
public:
    // MOCK_METHOD signature now correctly uses the new interface
    MOCK_METHOD(void, execute_buy, (const std::string& symbol, int quantity, double price), (override));
    MOCK_METHOD(void, execute_sell, (const std::string& symbol, int quantity, double price), (override));
    
    // You must also mock the other pure virtual functions from the interface
    MOCK_METHOD(int, get_position, (const std::string& symbol), (const, override));
    MOCK_METHOD(double, get_cash, (), (const, override));
    MOCK_METHOD(void, record_equity, (long long timestamp, (const std::map<std::string, double>&) market_prices), (override));
};

// Test fixture to hold our mock object
class SMACrossoverStrategyTest : public ::testing::Test {
protected:
    MockOrderManager mock_order_manager;
};

TEST_F(SMACrossoverStrategyTest, GeneratesBuySignalOnGoldenCross) {
    // 1. Arrange: Create the strategy with a short window of 3 and long of 5
    qse::SMACrossoverStrategy strategy(&mock_order_manager, 3, 5, std::chrono::minutes(1));

    // This specific price sequence will cause a crossover on the last bar.
    std::vector<double> prices = {100, 99, 98, 97, 96, 105, 106, 107};
    
    // 2. Expectation: We expect the execute_buy method to be called exactly once.
    // The `::testing::_` wildcard correctly matches the arguments.
    EXPECT_CALL(mock_order_manager, execute_buy(::testing::_, ::testing::_, ::testing::_)).Times(1);
    EXPECT_CALL(mock_order_manager, execute_sell(::testing::_, ::testing::_, ::testing::_)).Times(0);

    // 3. Act: Feed the prices to the strategy one by one
    for (double price : prices) {
        qse::Bar bar;
        bar.close = price;
        strategy.on_bar(bar);
    }
}