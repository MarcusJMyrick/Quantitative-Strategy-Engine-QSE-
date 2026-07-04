#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "qse/core/Config.h"
#include "qse/order/OrderManager.h"

using namespace qse;

namespace {

// Seed three ask levels: 100@10.0, 200@10.5, 300@11.0 (600 shares total)
void seed_asks(OrderBookFullDepth& book) {
    book.enqueue_order(Order::Side::SELL, 10.0, "mm_a", 100);
    book.enqueue_order(Order::Side::SELL, 10.5, "mm_b", 200);
    book.enqueue_order(Order::Side::SELL, 11.0, "mm_c", 300);
}

} // namespace

class DepthFillTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (const auto& f : files_) {
            std::filesystem::remove(f);
        }
    }

    std::unique_ptr<OrderManager> make_om(const std::string& tag) {
        std::string eq = "depth_eq_" + tag + ".csv";
        std::string tl = "depth_tl_" + tag + ".csv";
        files_.push_back(eq);
        files_.push_back(tl);
        auto om = std::make_unique<OrderManager>(100000.0, eq, tl);
        om->set_use_full_depth(true);
        return om;
    }

    std::vector<std::string> files_;
};

// A2: a small order fills entirely at the touch
TEST_F(DepthFillTest, SmallOrderFillsAtTouch) {
    auto om = make_om("small");
    seed_asks(om->depth_book("AAPL"));

    OrderId id = om->submit_market_order("AAPL", Order::Side::BUY, 50);
    om->attempt_fills();

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 50u);
    EXPECT_NEAR(order->avg_fill_price, 10.0, 1e-9);
    EXPECT_EQ(order->status, Order::Status::FILLED);
}

// A2 done-when: a large order pays more per share than a small one
// against the same book
TEST_F(DepthFillTest, LargeOrderPaysMorePerShareThanSmall) {
    auto om_small = make_om("cmp_small");
    auto om_large = make_om("cmp_large");
    seed_asks(om_small->depth_book("AAPL"));
    seed_asks(om_large->depth_book("AAPL")); // identical books

    OrderId small_id = om_small->submit_market_order("AAPL", Order::Side::BUY, 50);
    om_small->attempt_fills();
    OrderId large_id = om_large->submit_market_order("AAPL", Order::Side::BUY, 400);
    om_large->attempt_fills();

    auto small_order = om_small->get_order(small_id);
    auto large_order = om_large->get_order(large_id);
    ASSERT_TRUE(small_order.has_value());
    ASSERT_TRUE(large_order.has_value());
    ASSERT_EQ(small_order->filled_quantity, 50u);
    ASSERT_EQ(large_order->filled_quantity, 400u);

    EXPECT_NEAR(small_order->avg_fill_price, 10.0, 1e-9);
    // VWAP = (100*10.0 + 200*10.5 + 100*11.0) / 400 = 10.5
    EXPECT_NEAR(large_order->avg_fill_price, 10.5, 1e-9);
    EXPECT_GT(large_order->avg_fill_price, small_order->avg_fill_price);
}

// An order bigger than the book fills what liquidity exists and stays open
TEST_F(DepthFillTest, PartialFillWhenBookExhausted) {
    auto om = make_om("partial");
    seed_asks(om->depth_book("AAPL"));

    OrderId id = om->submit_market_order("AAPL", Order::Side::BUY, 700);
    om->attempt_fills();

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 600u); // the whole book
    // VWAP = (100*10.0 + 200*10.5 + 300*11.0) / 600 = 6400 / 600
    EXPECT_NEAR(order->avg_fill_price, 6400.0 / 600.0, 1e-9);
    EXPECT_EQ(order->status, Order::Status::PARTIALLY_FILLED);
}

// SELL orders walk the bid side downward
TEST_F(DepthFillTest, SellOrderWalksBids) {
    auto om = make_om("sell");
    auto& book = om->depth_book("AAPL");
    book.enqueue_order(Order::Side::BUY, 10.0, "mm_x", 100);
    book.enqueue_order(Order::Side::BUY, 9.5, "mm_y", 200);

    OrderId id = om->submit_market_order("AAPL", Order::Side::SELL, 300);
    om->attempt_fills();

    auto order = om->get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->filled_quantity, 300u);
    // VWAP = (100*10.0 + 200*9.5) / 300 = 2900 / 300
    EXPECT_NEAR(order->avg_fill_price, 2900.0 / 300.0, 1e-9);
}

// A2 done-when: the flag toggles between fill models on the same tick stream
TEST_F(DepthFillTest, FlagTogglesFillModel) {
    Tick tick;
    tick.symbol = "AAPL";
    tick.timestamp = from_unix_ms(1000);
    tick.price = 10.0;
    tick.bid = 9.9;
    tick.ask = 10.1;
    tick.bid_size = 1000;
    tick.ask_size = 1000;
    tick.volume = 1000;

    // Flag off (legacy tick fallback): market buy fills at the mid price
    auto om_legacy = make_om("flag_off");
    om_legacy->set_use_full_depth(false);
    OrderId legacy_id = om_legacy->submit_market_order("AAPL", Order::Side::BUY, 100);
    om_legacy->process_tick(tick);

    // Flag on: the tick builds synthetic depth and the buy walks the ask side
    auto om_depth = make_om("flag_on");
    OrderId depth_id = om_depth->submit_market_order("AAPL", Order::Side::BUY, 100);
    om_depth->process_tick(tick);

    auto legacy_order = om_legacy->get_order(legacy_id);
    auto depth_order = om_depth->get_order(depth_id);
    ASSERT_TRUE(legacy_order.has_value());
    ASSERT_TRUE(depth_order.has_value());
    ASSERT_EQ(legacy_order->filled_quantity, 100u);
    ASSERT_EQ(depth_order->filled_quantity, 100u);

    EXPECT_NEAR(legacy_order->avg_fill_price, 10.0, 1e-9); // mid price
    EXPECT_NEAR(depth_order->avg_fill_price, 10.1, 1e-9);  // paid the ask
}

// The YAML fill_model key parses and drives the OrderManager mode
TEST(FillModelConfigTest, ConfigFlagTogglesModel) {
    const std::string path = "fill_model_test_config.yaml";
    {
        std::ofstream out(path);
        out << "backtester:\n"
            << "  initial_cash: 50000.0\n"
            << "  fill_model: full_depth\n";
    }
    Config config;
    ASSERT_TRUE(config.load_config(path));
    std::filesystem::remove(path);

    EXPECT_EQ(config.get_fill_model(), "full_depth");
    EXPECT_TRUE(config.use_full_depth_book());

    {
        OrderManager om(config, "fm_eq.csv", "fm_tl.csv");
        EXPECT_TRUE(om.use_full_depth());
    }
    std::filesystem::remove("fm_eq.csv");
    std::filesystem::remove("fm_tl.csv");

    // Default (no fill_model key) stays on the legacy model
    Config default_config;
    EXPECT_EQ(default_config.get_fill_model(), "top_of_book");
    EXPECT_FALSE(default_config.use_full_depth_book());
}
