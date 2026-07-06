// E1: the venue-agnostic execution interface. Contract tests through the
// gmock handler prove callers need only IExecutionHandler; integration tests
// prove SimulatedExecutionHandler routes through the real backtest fill
// logic (full-depth book) with fills flowing back through the callback.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "qse/exe/IExecutionHandler.h"
#include "qse/exe/SimulatedExecutionHandler.h"
#include "qse/order/OrderManager.h"
#include "mocks/MockExecutionHandler.h"

#include <cstdio>
#include <vector>

using ::testing::Return;
using ::testing::StrictMock;

namespace {

// A caller written purely against the interface: brings a flat book to the
// target position with one market order. This is the shape strategies and
// the live runner (E3) use - no dependency on any concrete venue.
qse::OrderId route_to_target(qse::IExecutionHandler& exec, const std::string& symbol,
                             qse::Volume current, qse::Volume target) {
    if (target == current) {
        return {};
    }
    auto side = target > current ? qse::Order::Side::BUY : qse::Order::Side::SELL;
    auto qty = target > current ? target - current : current - target;
    return exec.submit_market_order(symbol, side, qty);
}

qse::Tick make_tick(double bid, double ask) {
    qse::Tick tick;
    tick.symbol = "TEST";
    tick.timestamp = qse::from_unix_ms(1748318400000);
    tick.price = (bid + ask) / 2.0;
    tick.bid = bid;
    tick.ask = ask;
    tick.bid_size = 1000;
    tick.ask_size = 1000;
    tick.volume = 0; // quote update only - no trade print
    return tick;
}

class SimulatedExecutionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        om_ = std::make_unique<qse::OrderManager>(100000.0, "exec_test_equity.csv",
                                                  "exec_test_tradelog.csv");
        om_->set_use_full_depth(true);
        exec_ = std::make_unique<qse::SimulatedExecutionHandler>(*om_);
        exec_->set_fill_callback([this](const qse::Fill& fill) { fills_.push_back(fill); });
    }

    void TearDown() override {
        exec_.reset();
        om_.reset();
        std::remove("exec_test_equity.csv");
        std::remove("exec_test_tradelog.csv");
    }

    std::unique_ptr<qse::OrderManager> om_;
    std::unique_ptr<qse::SimulatedExecutionHandler> exec_;
    std::vector<qse::Fill> fills_;
};

} // namespace

// --- Contract tests: callers depend only on the interface ---

TEST(ExecutionHandlerContractTest, CallersNeedOnlyTheInterface) {
    StrictMock<qse::MockExecutionHandler> mock;
    EXPECT_CALL(mock, submit_market_order("TEST", qse::Order::Side::BUY, 500))
        .WillOnce(Return("venue-1"));

    qse::OrderId id = route_to_target(mock, "TEST", 0, 500);
    EXPECT_EQ(id, "venue-1");
}

TEST(ExecutionHandlerContractTest, FlatteningRoutesASell) {
    StrictMock<qse::MockExecutionHandler> mock;
    EXPECT_CALL(mock, submit_market_order("TEST", qse::Order::Side::SELL, 300))
        .WillOnce(Return("venue-2"));
    EXPECT_CALL(mock, cancel_order(qse::OrderId("venue-2"))).WillOnce(Return(true));

    qse::OrderId id = route_to_target(mock, "TEST", 300, 0);
    EXPECT_EQ(id, "venue-2");
    EXPECT_TRUE(mock.cancel_order(id)); // e.g. a timeout guard cancelling
}

TEST(ExecutionHandlerContractTest, NoOrderWhenAlreadyAtTarget) {
    StrictMock<qse::MockExecutionHandler> mock; // no expectations: no calls allowed
    EXPECT_TRUE(route_to_target(mock, "TEST", 250, 250).empty());
}

// --- Integration: the simulated venue is the real backtest engine ---

TEST_F(SimulatedExecutionHandlerTest, MarketOrderFillsThroughRealEngine) {
    qse::OrderId id = exec_->submit_market_order("TEST", qse::Order::Side::BUY, 100);
    ASSERT_FALSE(id.empty());

    om_->process_tick(make_tick(99.9, 100.1));
    om_->attempt_fills();

    ASSERT_EQ(fills_.size(), 1u);
    EXPECT_EQ(fills_[0].symbol, "TEST");
    EXPECT_EQ(fills_[0].quantity, 100);
    EXPECT_DOUBLE_EQ(fills_[0].price, 100.1); // paid the ask, not the mid
    EXPECT_EQ(om_->get_position("TEST"), 100);

    auto order = exec_->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
}

TEST_F(SimulatedExecutionHandlerTest, RestingLimitOrderCancelsCleanly) {
    om_->process_tick(make_tick(99.9, 100.1));

    // Buy limit below the bid: rests in the book, never marketable here
    qse::OrderId id = exec_->submit_limit_order("TEST", qse::Order::Side::BUY, 100, 99.5,
                                                qse::Order::TimeInForce::DAY);
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(exec_->get_order(id)->status, qse::Order::Status::PENDING);

    EXPECT_TRUE(exec_->cancel_order(id));
    EXPECT_EQ(exec_->get_order(id)->status, qse::Order::Status::CANCELLED);

    om_->process_tick(make_tick(99.9, 100.1));
    om_->attempt_fills();
    EXPECT_TRUE(fills_.empty()); // a cancelled order never fills
    EXPECT_EQ(om_->get_position("TEST"), 0);
}

TEST_F(SimulatedExecutionHandlerTest, ReplacePreservesSymbolAndSide) {
    om_->process_tick(make_tick(99.9, 100.1));
    qse::OrderId id = exec_->submit_limit_order("TEST", qse::Order::Side::BUY, 100, 99.5,
                                                qse::Order::TimeInForce::DAY);

    qse::OrderId new_id = exec_->replace_order(id, 200, 99.8);
    ASSERT_FALSE(new_id.empty());
    EXPECT_NE(new_id, id);

    EXPECT_EQ(exec_->get_order(id)->status, qse::Order::Status::CANCELLED);
    auto replaced = exec_->get_order(new_id);
    ASSERT_TRUE(replaced.has_value());
    EXPECT_EQ(replaced->symbol, "TEST");
    EXPECT_EQ(replaced->side, qse::Order::Side::BUY);
    EXPECT_EQ(replaced->quantity, 200);
    EXPECT_DOUBLE_EQ(replaced->limit_price, 99.8);
    EXPECT_TRUE(replaced->is_active());
}

TEST_F(SimulatedExecutionHandlerTest, ReplaceRejectsMarketOrdersAndUnknownIds) {
    qse::OrderId market_id = exec_->submit_market_order("TEST", qse::Order::Side::BUY, 100);
    EXPECT_TRUE(exec_->replace_order(market_id, 200, 100.0).empty());
    EXPECT_TRUE(exec_->replace_order("no-such-order", 200, 100.0).empty());
}

TEST_F(SimulatedExecutionHandlerTest, HandlerMatchesDirectOrderManagerExactly) {
    // The same order sequence submitted directly to a second OrderManager
    // must produce identical cash and position - the handler adds routing,
    // not behavior
    qse::OrderManager direct(100000.0, "exec_direct_equity.csv", "exec_direct_tradelog.csv");
    direct.set_use_full_depth(true);

    exec_->submit_market_order("TEST", qse::Order::Side::BUY, 100);
    direct.submit_market_order("TEST", qse::Order::Side::BUY, 100);

    for (auto* om : {om_.get(), &direct}) {
        om->process_tick(make_tick(99.9, 100.1));
        om->attempt_fills();
        om->process_tick(make_tick(100.0, 100.2));
        om->attempt_fills();
    }

    EXPECT_EQ(om_->get_position("TEST"), direct.get_position("TEST"));
    EXPECT_DOUBLE_EQ(om_->get_cash(), direct.get_cash());

    std::remove("exec_direct_equity.csv");
    std::remove("exec_direct_tradelog.csv");
}
