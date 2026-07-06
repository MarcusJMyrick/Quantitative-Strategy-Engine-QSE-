#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "qse/order/OrderManager.h"

using namespace qse;

namespace {

// A trade print: price and volume set, no quote sizes (so the synthetic
// quote refresh does not add or remove displayed liquidity)
Tick trade_tick(const std::string& symbol, Price price, Volume volume) {
    Tick tick{};
    tick.symbol = symbol;
    tick.timestamp = from_unix_ms(1000);
    tick.price = price;
    tick.volume = volume;
    return tick;
}

// A quote-only tick: bid/ask with displayed sizes, no trade
Tick quote_tick(const std::string& symbol, Price bid, Volume bid_size, Price ask, Volume ask_size) {
    Tick tick{};
    tick.symbol = symbol;
    tick.timestamp = from_unix_ms(1000);
    tick.bid = bid;
    tick.bid_size = bid_size;
    tick.ask = ask;
    tick.ask_size = ask_size;
    return tick;
}

} // namespace

class LimitQueueFillTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (const auto& f : files_) {
            std::filesystem::remove(f);
        }
    }

    std::unique_ptr<OrderManager> make_om(const std::string& tag) {
        std::string eq = "lq_eq_" + tag + ".csv";
        std::string tl = "lq_tl_" + tag + ".csv";
        files_.push_back(eq);
        files_.push_back(tl);
        auto om = std::make_unique<OrderManager>(100000.0, eq, tl);
        om->set_use_full_depth(true);
        return om;
    }

    std::vector<std::string> files_;
};

// A3 done-when (a): an order behind the queue does not fill when a small
// trade prints at its price
TEST_F(LimitQueueFillTest, NoFillWhileQueueAhead) {
    auto om = make_om("queue_ahead");
    om->depth_book("AAPL").enqueue_order(Order::Side::SELL, 10.0, "mm_1", 300);

    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::SELL, 100, 10.0, Order::TimeInForce::GTC);
    // Joined the back of the queue, behind the 300 already displayed
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 2u);

    // A 200-share trade prints: it only chews into the queue ahead
    om->process_tick(trade_tick("AAPL", 10.0, 200));

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 0u);
    EXPECT_EQ(order->status, Order::Status::PENDING);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 2u);
}

// A3 done-when (b): fills once the queue ahead is exhausted
TEST_F(LimitQueueFillTest, FillsAfterQueueAheadExhausted) {
    auto om = make_om("queue_fill");
    om->depth_book("AAPL").enqueue_order(Order::Side::SELL, 10.0, "mm_1", 300);

    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::SELL, 100, 10.0, Order::TimeInForce::GTC);

    // 200 consumes queue ahead down to 100; still nothing for us
    om->process_tick(trade_tick("AAPL", 10.0, 200));
    // 150 finishes the 100 ahead, then reaches 50 of ours
    om->process_tick(trade_tick("AAPL", 10.0, 150));

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 50u);
    EXPECT_NEAR(order->avg_fill_price, 10.0, 1e-9);
    EXPECT_EQ(order->status, Order::Status::PARTIALLY_FILLED);
    // We are at the head of the queue now
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 1u);

    // The rest fills on the next print
    om->process_tick(trade_tick("AAPL", 10.0, 60));
    order = om->get_order(id);
    EXPECT_EQ(order->filled_quantity, 100u);
    EXPECT_EQ(order->status, Order::Status::FILLED);
}

// A3 done-when (c): cancel removes the order from the queue
TEST_F(LimitQueueFillTest, CancelRemovesFromQueue) {
    auto om = make_om("cancel");
    om->depth_book("AAPL").enqueue_order(Order::Side::SELL, 10.0, "mm_1", 100);

    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::SELL, 50, 10.0, Order::TimeInForce::GTC);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 2u);

    EXPECT_TRUE(om->cancel_order(id));
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 0u);

    // A trade big enough to have reached us fills nothing of ours
    om->process_tick(trade_tick("AAPL", 10.0, 200));

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 0u);
    EXPECT_EQ(order->status, Order::Status::CANCELLED);
}

// FIFO priority among strategy orders at the same level
TEST_F(LimitQueueFillTest, FifoAmongStrategyOrders) {
    auto om = make_om("fifo");

    OrderId first =
        om->submit_limit_order("AAPL", Order::Side::SELL, 50, 10.0, Order::TimeInForce::GTC);
    OrderId second =
        om->submit_limit_order("AAPL", Order::Side::SELL, 50, 10.0, Order::TimeInForce::GTC);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, first), 1u);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, second), 2u);

    om->process_tick(trade_tick("AAPL", 10.0, 70));

    auto first_order = om->get_order(first);
    auto second_order = om->get_order(second);
    ASSERT_TRUE(first_order.has_value());
    ASSERT_TRUE(second_order.has_value());
    EXPECT_EQ(first_order->filled_quantity, 50u);
    EXPECT_EQ(first_order->status, Order::Status::FILLED);
    EXPECT_EQ(second_order->filled_quantity, 20u);
    EXPECT_EQ(second_order->status, Order::Status::PARTIALLY_FILLED);
}

// A marketable limit takes liquidity up to its limit price, then rests
TEST_F(LimitQueueFillTest, MarketableLimitTakesThenRests) {
    auto om = make_om("marketable");
    auto& book = om->depth_book("AAPL");
    book.enqueue_order(Order::Side::SELL, 10.0, "mm_a", 100);
    book.enqueue_order(Order::Side::SELL, 10.5, "mm_b", 200);

    // Buy 150 limit 10.2: takes the 100 @ 10.0, will not lift 10.5,
    // rests the remaining 50 on the bid side at 10.2
    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::BUY, 150, 10.2, Order::TimeInForce::GTC);

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 100u);
    EXPECT_NEAR(order->avg_fill_price, 10.0, 1e-9);
    EXPECT_EQ(order->status, Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(book.queue_position(Order::Side::BUY, 10.2, id), 1u);
}

// An IOC limit takes what it can and never rests
TEST_F(LimitQueueFillTest, IocTakesAndCancelsRemainder) {
    auto om = make_om("ioc");
    om->depth_book("AAPL").enqueue_order(Order::Side::SELL, 10.0, "mm_a", 100);

    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::BUY, 150, 10.2, Order::TimeInForce::IOC);

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 100u);
    EXPECT_EQ(order->status, Order::Status::CANCELLED);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::BUY, 10.2, id), 0u);
}

// A strategy market order consuming a resting strategy limit credits both
// sides of the trade (fill attribution through the book)
TEST_F(LimitQueueFillTest, MakerCreditedWhenStrategyMarketOrderConsumesIt) {
    auto om = make_om("attribution");

    OrderId maker_id =
        om->submit_limit_order("AAPL", Order::Side::SELL, 100, 10.0, Order::TimeInForce::GTC);
    OrderId taker_id = om->submit_market_order("AAPL", Order::Side::BUY, 60);
    om->attempt_fills();

    auto maker = om->get_order(maker_id);
    auto taker = om->get_order(taker_id);
    ASSERT_TRUE(maker.has_value());
    ASSERT_TRUE(taker.has_value());

    EXPECT_EQ(taker->filled_quantity, 60u);
    EXPECT_NEAR(taker->avg_fill_price, 10.0, 1e-9);
    EXPECT_EQ(taker->status, Order::Status::FILLED);

    EXPECT_EQ(maker->filled_quantity, 60u);
    EXPECT_NEAR(maker->avg_fill_price, 10.0, 1e-9);
    EXPECT_EQ(maker->status, Order::Status::PARTIALLY_FILLED);
    // The maker's remaining 40 still holds the front of the queue
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, maker_id), 1u);
}

// End-to-end through the tick pipeline: displayed quote size stays ahead of
// the resting order across quote refreshes, and trade prints advance it
TEST_F(LimitQueueFillTest, QuoteRefreshKeepsDisplayedLiquidityAhead) {
    auto om = make_om("pipeline");

    // Quote: 300 displayed at the 10.0 ask
    om->process_tick(quote_tick("AAPL", 9.9, 500, 10.0, 300));

    OrderId id =
        om->submit_limit_order("AAPL", Order::Side::SELL, 100, 10.0, Order::TimeInForce::GTC);
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 2u);

    // A quote refresh must not let us jump ahead of displayed size
    om->process_tick(quote_tick("AAPL", 9.9, 500, 10.0, 300));
    EXPECT_EQ(om->depth_book("AAPL").queue_position(Order::Side::SELL, 10.0, id), 2u);

    // A 350 trade consumes the 300 displayed, then 50 of ours
    om->process_tick(trade_tick("AAPL", 10.0, 350));
    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 50u);
    EXPECT_EQ(order->status, Order::Status::PARTIALLY_FILLED);

    // The remainder fills on the next print
    om->process_tick(trade_tick("AAPL", 10.0, 50));
    order = om->get_order(id);
    EXPECT_EQ(order->filled_quantity, 100u);
    EXPECT_EQ(order->status, Order::Status::FILLED);
}
