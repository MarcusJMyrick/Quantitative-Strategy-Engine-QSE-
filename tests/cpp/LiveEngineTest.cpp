// E3: LiveEngine unit tests - ticks in via a test ring, orders out via the
// mock venue, fills recorded, reconciliation checked both ways. Plus the
// Alpaca market-data feed parsing/dedup against a fake HTTP client.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "qse/core/SPSCRingBuffer.h"
#include "qse/live/AlpacaMarketDataFeed.h"
#include "qse/live/LiveEngine.h"
#include "mocks/MockExecutionHandler.h"

#include <atomic>
#include <memory>
#include <vector>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

// Price path: down-trend long enough to fill both SMAs, then a sharp
// up-trend. With SMA 2/3 on 1s bars this produces exactly one golden cross.
const std::vector<double> kCrossPath = {100, 99, 98, 97, 96, 95, 101, 104, 107, 110};

qse::Tick tick_at(double price, int second) {
    qse::Tick tick;
    tick.symbol = "TEST";
    tick.timestamp = qse::from_unix_ms(1748318400000 + second * 1000LL);
    tick.price = price;
    tick.bid = price - 0.01;
    tick.ask = price + 0.01;
    tick.bid_size = 100;
    tick.ask_size = 100;
    tick.volume = 0;
    return tick;
}

class LiveEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        ring_ = std::make_unique<qse::SPSCRingBuffer<qse::Tick>>(256);
        EXPECT_CALL(exec_, set_fill_callback(_)).WillOnce(SaveArg<0>(&fill_callback_));
        qse::LiveEngineConfig config;
        config.symbol = "TEST";
        config.bar_interval = std::chrono::seconds(1);
        config.sma_short = 2;
        config.sma_long = 3;
        config.order_size = 5;
        engine_ = std::make_unique<qse::LiveEngine>(config, exec_, [this](const auto& handler) {
            return ring_->consume_all([&handler](qse::Tick&& tick) { handler(tick); });
        });
    }

    void push_path(const std::vector<double>& prices) {
        int second = 0;
        for (double price : prices) {
            ASSERT_TRUE(ring_->try_push(tick_at(price, second++)));
        }
        // One tick past the end so the final bar closes
        ASSERT_TRUE(ring_->try_push(tick_at(prices.back(), second)));
    }

    NiceMock<qse::MockExecutionHandler> exec_;
    qse::IExecutionHandler::FillCallback fill_callback_;
    std::unique_ptr<qse::SPSCRingBuffer<qse::Tick>> ring_;
    std::unique_ptr<qse::LiveEngine> engine_;
};

} // namespace

TEST_F(LiveEngineTest, GoldenCrossSubmitsOneBuy) {
    EXPECT_CALL(exec_, submit_market_order("TEST", qse::Order::Side::BUY, 5))
        .WillOnce(Return("live-1"));
    EXPECT_CALL(exec_, poll_fills()).Times(::testing::AtLeast(1));

    push_path(kCrossPath);
    std::size_t processed = engine_->step();

    EXPECT_EQ(processed, kCrossPath.size() + 1);
    EXPECT_EQ(engine_->bars_seen(), kCrossPath.size());
    ASSERT_EQ(engine_->submitted_orders().size(), 1u);
    EXPECT_EQ(engine_->submitted_orders()[0], "live-1");
}

TEST_F(LiveEngineTest, FillsAreRecordedLocally) {
    EXPECT_CALL(exec_, submit_market_order(_, _, _)).WillOnce(Return("live-1"));
    push_path(kCrossPath);
    engine_->step();

    ASSERT_TRUE(fill_callback_); // engine registered its callback
    fill_callback_(qse::Fill("live-1", "TEST", 5, 101.0, std::chrono::system_clock::now(), "BUY"));

    ASSERT_EQ(engine_->local_fills().size(), 1u);
    EXPECT_EQ(engine_->local_fills()[0].quantity, 5);
    EXPECT_DOUBLE_EQ(engine_->local_fills()[0].price, 101.0);
}

TEST_F(LiveEngineTest, ReconciliationMatchesWhenVenueAgrees) {
    EXPECT_CALL(exec_, submit_market_order(_, _, _)).WillOnce(Return("live-1"));
    push_path(kCrossPath);
    engine_->step();
    fill_callback_(qse::Fill("live-1", "TEST", 5, 101.0, std::chrono::system_clock::now(), "BUY"));

    qse::Order venue_view;
    venue_view.order_id = "live-1";
    venue_view.filled_quantity = 5;
    EXPECT_CALL(exec_, get_order(qse::OrderId("live-1")))
        .WillOnce(Return(std::optional<qse::Order>(venue_view)));

    auto report = engine_->reconcile();
    ASSERT_EQ(report.lines.size(), 1u);
    EXPECT_TRUE(report.lines[0].matched);
    EXPECT_TRUE(report.all_matched);
}

TEST_F(LiveEngineTest, ReconciliationFlagsMissingLocalFills) {
    EXPECT_CALL(exec_, submit_market_order(_, _, _)).WillOnce(Return("live-1"));
    push_path(kCrossPath);
    engine_->step();
    // Venue filled 5 but the local log never saw the fill

    qse::Order venue_view;
    venue_view.order_id = "live-1";
    venue_view.filled_quantity = 5;
    EXPECT_CALL(exec_, get_order(qse::OrderId("live-1")))
        .WillOnce(Return(std::optional<qse::Order>(venue_view)));

    auto report = engine_->reconcile();
    ASSERT_EQ(report.lines.size(), 1u);
    EXPECT_EQ(report.lines[0].local_fill_qty, 0);
    EXPECT_EQ(report.lines[0].venue_fill_qty, 5);
    EXPECT_FALSE(report.all_matched);
}

TEST_F(LiveEngineTest, IgnoresTicksForOtherSymbols) {
    EXPECT_CALL(exec_, submit_market_order(_, _, _)).Times(0);
    qse::Tick foreign = tick_at(100.0, 0);
    foreign.symbol = "OTHER";
    ASSERT_TRUE(ring_->try_push(foreign));
    engine_->step();
    EXPECT_EQ(engine_->bars_seen(), 0u);
}

// --- Alpaca market-data feed: parsing and de-duplication ---

namespace {

class FakeHttpClient : public qse::IHttpClient {
public:
    explicit FakeHttpClient(std::vector<qse::HttpResponse> responses)
        : responses_(std::move(responses)) {}

    qse::HttpResponse get(const std::string& url, const std::vector<std::string>&) override {
        last_url = url;
        std::size_t i = index_.fetch_add(1);
        return i < responses_.size() ? responses_[i] : responses_.back();
    }
    qse::HttpResponse post(const std::string&, const std::vector<std::string>&,
                           const std::string&) override {
        return {500, "unexpected"};
    }
    qse::HttpResponse patch(const std::string&, const std::vector<std::string>&,
                            const std::string&) override {
        return {500, "unexpected"};
    }
    qse::HttpResponse del(const std::string&, const std::vector<std::string>&) override {
        return {500, "unexpected"};
    }

    std::string last_url;

private:
    std::vector<qse::HttpResponse> responses_;
    std::atomic<std::size_t> index_{0};
};

} // namespace

TEST(AlpacaMarketDataFeedTest, ParsesQuotesAndDeduplicates) {
    auto http = std::make_shared<FakeHttpClient>(std::vector<qse::HttpResponse>{
        {200,
         R"({"symbol":"AAPL","quote":{"t":"2026-07-06T14:30:00.1Z","bp":199.2,"ap":199.3,"bs":3,"as":2}})"},
        {200,
         R"({"symbol":"AAPL","quote":{"t":"2026-07-06T14:30:00.1Z","bp":199.2,"ap":199.3,"bs":3,"as":2}})"},
        {200,
         R"({"symbol":"AAPL","quote":{"t":"2026-07-06T14:30:01.4Z","bp":199.4,"ap":199.6,"bs":1,"as":4}})"},
        {200, R"({"message":"no quote"})"},
    });
    qse::AlpacaMarketDataFeed feed(http, "k", "s", "AAPL");

    EXPECT_TRUE(feed.poll_once());  // fresh quote
    EXPECT_FALSE(feed.poll_once()); // identical timestamp: deduplicated
    EXPECT_TRUE(feed.poll_once());  // new quote
    EXPECT_FALSE(feed.poll_once()); // malformed payload tolerated

    EXPECT_TRUE(http->last_url.find("/v2/stocks/AAPL/quotes/latest") != std::string::npos);

    std::vector<qse::Tick> ticks;
    feed.drain([&ticks](const qse::Tick& tick) { ticks.push_back(tick); });
    ASSERT_EQ(ticks.size(), 2u);
    EXPECT_DOUBLE_EQ(ticks[0].bid, 199.2);
    EXPECT_DOUBLE_EQ(ticks[0].ask, 199.3);
    EXPECT_DOUBLE_EQ(ticks[0].price, 199.25); // mid
    EXPECT_EQ(ticks[0].symbol, "AAPL");
    EXPECT_DOUBLE_EQ(ticks[1].price, 199.5);
    EXPECT_EQ(feed.dropped_ticks(), 0u);
}
