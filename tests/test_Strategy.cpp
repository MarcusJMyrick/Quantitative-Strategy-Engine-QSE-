#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "IOrderManager.h"
#include "SMACrossoverStrategy.h"
#include "Data.h"
#include <vector>

// Mock Order Manager using Google Mock
class MockOrderManager : public qse::IOrderManager {
public:
    MOCK_METHOD(void, execute_buy, (double price), (override));
    MOCK_METHOD(void, execute_sell, (double price), (override));
};

// Test fixture
class SMACrossoverStrategyTest : public ::testing::Test {
protected:
    MockOrderManager mock_order_manager;
};

TEST_F(SMACrossoverStrategyTest, GeneratesBuySignalOnGoldenCross) {
    // Arrange: short window 3, long window 5
    qse::SMACrossoverStrategy strategy(&mock_order_manager, 3, 5);
    std::vector<double> prices = {100, 99, 98, 97, 96, 105, 106, 107};

    // Expect execute_buy to be called once, execute_sell never
    EXPECT_CALL(mock_order_manager, execute_buy(::testing::_)).Times(1);
    EXPECT_CALL(mock_order_manager, execute_sell(::testing::_)).Times(0);

    // Act: Feed prices to the strategy
    for (double price : prices) {
        qse::Bar bar;
        bar.close = price;
        strategy.on_bar(bar);
    }
} 