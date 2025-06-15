#include <gtest/gtest.h>
#include "OrderManager.h"

namespace qse {
namespace test {

class OrderManagerTest : public ::testing::Test {
protected:
    OrderManagerTest() : manager(100000.0, 1.0, 0.0) {
        bar.close = 100.0;
    }
    OrderManager manager;
    Bar bar;
};

TEST_F(OrderManagerTest, InitialState) {
    EXPECT_EQ(manager.get_position(), 0);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100000.0);
}

TEST_F(OrderManagerTest, ExecuteBuy) {
    manager.execute_buy(bar);
    EXPECT_EQ(manager.get_position(), 1);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 99900.0);
}

TEST_F(OrderManagerTest, ExecuteSell) {
    manager.execute_buy(bar);
    manager.execute_sell(bar);
    EXPECT_EQ(manager.get_position(), 0);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100100.0);
}

TEST_F(OrderManagerTest, MultipleTrades) {
    manager.execute_buy(bar);   // Position: 1, Cash: 99900
    manager.execute_buy(bar);   // Position: 2, Cash: 99799
    manager.execute_sell(bar);  // Position: 1, Cash: 99901
    manager.execute_sell(bar);  // Position: 0, Cash: 100004
    
    EXPECT_EQ(manager.get_position(), 0);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100004.0);
}

TEST(OrderManagerTest, InitialStateWithParams) {
    OrderManager om(100000.0, 1.0, 0.01);
    EXPECT_EQ(om.get_position(), 0);
    EXPECT_DOUBLE_EQ(om.get_portfolio_value(100.0), 100000.0);
}

TEST(OrderManagerTest, BasicBuySell) {
    OrderManager om(100000.0, 1.0, 0.01);
    Bar test_bar;
    test_bar.close = 100.0;

    om.execute_buy(test_bar);
    EXPECT_EQ(om.get_position(), 1);
    EXPECT_DOUBLE_EQ(om.get_portfolio_value(100.0), 100000.0);

    om.execute_sell(test_bar);
    EXPECT_EQ(om.get_position(), 0);
    EXPECT_DOUBLE_EQ(om.get_portfolio_value(100.0), 100000.0);
}

TEST(OrderManagerCostTest, AccountsForCommissionAndSlippage) {
    // Arrange: Set up a test scenario with known costs
    OrderManager om(100000.0, 1.0, 0.05); // Cash: 100k, Commission: $1, Slippage: 5 cents
    Bar test_bar;
    test_bar.close = 100.0;

    // Act: Execute a buy
    om.execute_buy(test_bar);

    // Assert: Check if the final cash is exactly what we expect
    // Execution price = 100.0 (close) + 0.05 (slippage) = 100.05
    // Total cost = 100.05 (price) + 1.0 (commission) = 101.05
    // Final cash = 100000.0 - 101.05 = 99898.95
    EXPECT_DOUBLE_EQ(om.get_cash(), 99898.95);
    EXPECT_EQ(om.get_position(), 1);
}

} // namespace test
} // namespace qse 