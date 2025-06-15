#include <gtest/gtest.h>
#include "OrderManager.h"

namespace qse {
namespace test {

class OrderManagerTest : public ::testing::Test {
protected:
    OrderManager manager;
};

TEST_F(OrderManagerTest, InitialState) {
    EXPECT_EQ(manager.get_position(), 0);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100000.0);
    EXPECT_DOUBLE_EQ(manager.get_pnl(), 0.0);
}

TEST_F(OrderManagerTest, ExecuteBuy) {
    manager.execute_buy(100.0);
    EXPECT_EQ(manager.get_position(), 1);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 99900.0);
}

TEST_F(OrderManagerTest, ExecuteSell) {
    manager.execute_sell(100.0);
    EXPECT_EQ(manager.get_position(), -1);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100100.0);
}

TEST_F(OrderManagerTest, MultipleTrades) {
    manager.execute_buy(100.0);   // Position: 1, Cash: 99900
    manager.execute_buy(101.0);   // Position: 2, Cash: 99799
    manager.execute_sell(102.0);  // Position: 1, Cash: 99901
    manager.execute_sell(103.0);  // Position: 0, Cash: 100004
    
    EXPECT_EQ(manager.get_position(), 0);
    EXPECT_DOUBLE_EQ(manager.get_cash(), 100004.0);
}

} // namespace test
} // namespace qse 