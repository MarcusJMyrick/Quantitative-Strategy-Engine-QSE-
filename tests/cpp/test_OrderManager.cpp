#include <gtest/gtest.h>
#include "qse/order/OrderManager.h"
#include "qse/data/Data.h"
#include <memory>

// This is the standard, correct way to write a test fixture in Google Test.
class OrderManagerTest : public ::testing::Test {
protected:
    // SetUp() is called by Google Test before each and every test case.
    void SetUp() override {
        // We create a fresh, correctly initialized manager for each test.
        // This ensures tests are isolated and prevents memory errors.
        manager = std::make_unique<qse::OrderManager>(100000.0, 1.0, 0.01);
    }

    // The smart pointer will automatically manage the object's memory.
    std::unique_ptr<qse::OrderManager> manager;
};

// Use TEST_F for all tests to access the 'manager' from the fixture.
// Note the -> arrow operator is used because manager is now a pointer.
TEST_F(OrderManagerTest, InitialState) {
    EXPECT_EQ(manager->get_position(), 0);
    EXPECT_DOUBLE_EQ(manager->get_cash(), 100000.0);
}

TEST_F(OrderManagerTest, ExecuteBuyWithCosts) {
    qse::Bar bar;
    bar.close = 100.0;
    manager->execute_buy(bar);

    // Expected cash = 100000 - execution_price(100.01) - commission(1.0) = 99898.99
    EXPECT_DOUBLE_EQ(manager->get_cash(), 99898.99);
    EXPECT_EQ(manager->get_position(), 1);
}

TEST_F(OrderManagerTest, ExecuteSellWithCosts) {
    qse::Bar bar;
    bar.close = 100.0;
    manager->execute_sell(bar);

    // Expected cash = 100000 + execution_price(99.99) - commission(1.0) = 100098.99
    EXPECT_DOUBLE_EQ(manager->get_cash(), 100098.99);
    EXPECT_EQ(manager->get_position(), -1);
}

TEST_F(OrderManagerTest, PortfolioValueIsCorrectAfterTrade) {
    qse::Bar bar;
    bar.close = 100.0;
    
    manager->execute_buy(bar); // Position: 1, Cash: 99898.99

    // Check portfolio value at a new price of 110.0
    // Expected value = 99898.99 (cash) + 1 * 110.0 (position value) = 100008.99
    EXPECT_DOUBLE_EQ(manager->get_portfolio_value(110.0), 100008.99);
}