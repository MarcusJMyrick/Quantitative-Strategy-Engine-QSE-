#include <gtest/gtest.h>
#include "Backtester.h"
#include "IDataReader.h"
#include "Data.h"
#include <memory>
#include <vector>

namespace qse {
namespace test {

// Simple mock DataReader that returns a single bar
class MockDataReader : public IDataReader {
public:
    std::vector<Bar> read_all_bars() override {
        Bar bar;
        bar.open = 100;
        bar.high = 105;
        bar.low = 95;
        bar.close = 102;
        bar.volume = 1000;
        return {bar};
    }

    size_t get_bar_count() const override {
        return 1;
    }

    std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time) override {
        return {};
    }
};

// Simple mock strategy that just counts bars
class MockStrategy : public IStrategy {
public:
    void on_bar(const Bar&) override { ++count_; }
    int count_ = 0;
};

// Simple mock order manager
class MockOrderManager : public OrderManager {
public:
    MockOrderManager() : OrderManager() {}
};

TEST(BacktesterTest, CanConstructAndRun) {
    auto data_reader = std::make_unique<MockDataReader>();
    auto strategy = std::make_unique<MockStrategy>();
    auto order_manager = std::make_unique<MockOrderManager>();

    qse::Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::move(order_manager)
    );

    // Should run without throwing
    EXPECT_NO_THROW(backtester.run());
}

} // namespace test
} // namespace qse 