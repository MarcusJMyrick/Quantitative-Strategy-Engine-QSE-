#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "qse/strategy/FactorStrategy.h"
#include "qse/core/Backtester.h"
#include "qse/order/OrderManager.h"
#include "qse/exe/FactorExecutionEngine.h"
#include "qse/data/IDataReader.h"
#include <memory>
#include <fstream>
#include <filesystem>
#include <chrono>

using namespace qse;
using ::testing::_;

// --------------------- In-memory tick reader ---------------------
class InMemoryTickReader : public IDataReader {
public:
    explicit InMemoryTickReader(std::vector<Tick> ticks) : ticks_(std::move(ticks)) {}
    const std::vector<Tick>& read_all_ticks() const override { return ticks_; }
    const std::vector<Bar>& read_all_bars() const override { return bars_; }
private:
    std::vector<Tick> ticks_;
    std::vector<Bar> bars_; // Empty – not needed for this test
};

class FactorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_weights_dir = "test_integration_weights";
        std::filesystem::create_directories(test_weights_dir);
    }
    void TearDown() override {
        std::filesystem::remove_all(test_weights_dir);
    }
    // Helper to create weight files for a date (YYYYMMDD)
    void create_weight_file(const std::string& date, double weight) {
        std::ofstream file(test_weights_dir + "/weights_" + date + ".csv");
        file << "symbol,weight\nAAPL," << weight << "\n";
    }
    // Helper to build a single dummy tick for a day with a given price
    Tick build_tick(const std::tm& tm, double price) {
        Tick t;
        std::time_t tt = std::mktime(const_cast<std::tm*>(&tm));
        t.timestamp = Timestamp(std::chrono::system_clock::from_time_t(tt));
        t.symbol = "AAPL";
        t.price = price;
        t.volume = 100;
        t.bid = price - 0.01;
        t.ask = price + 0.01;
        t.bid_size = 100;
        t.ask_size = 100;
        return t;
    }
    std::string test_weights_dir;
};

TEST_F(FactorIntegrationTest, FillsMatchWeights) {
    // Create weight files
    create_weight_file("20241215", 0.10); // +10%
    create_weight_file("20241216", -0.05); // -5%
    create_weight_file("20241217", 0.00);  // flat

    // Build ticks for 3 days (one tick per day is sufficient)
    std::vector<Tick> day1_ticks;
    std::vector<Tick> day2_ticks;
    std::vector<Tick> day3_ticks;

    std::tm tm_day1 = {};
    tm_day1.tm_year = 2024 - 1900; tm_day1.tm_mon = 11; tm_day1.tm_mday = 15;
    day1_ticks.push_back(build_tick(tm_day1, 100.0));

    std::tm tm_day2 = {};
    tm_day2.tm_year = 2024 - 1900; tm_day2.tm_mon = 11; tm_day2.tm_mday = 16;
    day2_ticks.push_back(build_tick(tm_day2, 101.0));

    std::tm tm_day3 = {};
    tm_day3.tm_year = 2024 - 1900; tm_day3.tm_mon = 11; tm_day3.tm_mday = 17;
    day3_ticks.push_back(build_tick(tm_day3, 102.0));

    // Shared OrderManager
    auto om = std::make_shared<OrderManager>(1000000.0, "eq_curve.csv", "trades.csv");

    ExecConfig cfg; cfg.order_style = "market"; cfg.min_qty = 1;

    // Day 1
    auto strat1 = std::make_unique<FactorStrategy>(om, "AAPL", test_weights_dir, 1.0, cfg);
    FactorStrategy* strat1_ptr = strat1.get();
    auto reader1 = std::make_unique<InMemoryTickReader>(day1_ticks);
    Backtester bt1("AAPL", std::move(reader1), std::move(strat1), om);
    bt1.run();
    strat1_ptr->on_day_close_with_prices(std::chrono::system_clock::from_time_t(std::mktime(&tm_day1)), {{"AAPL", 100.0}});

    // Day 2
    auto strat2 = std::make_unique<FactorStrategy>(om, "AAPL", test_weights_dir, 1.0, cfg);
    FactorStrategy* strat2_ptr = strat2.get();
    auto reader2 = std::make_unique<InMemoryTickReader>(day2_ticks);
    Backtester bt2("AAPL", std::move(reader2), std::move(strat2), om);
    bt2.run();
    strat2_ptr->on_day_close_with_prices(std::chrono::system_clock::from_time_t(std::mktime(&tm_day2)), {{"AAPL", 101.0}});

    // Day 3
    auto strat3 = std::make_unique<FactorStrategy>(om, "AAPL", test_weights_dir, 1.0, cfg);
    FactorStrategy* strat3_ptr = strat3.get();
    auto reader3 = std::make_unique<InMemoryTickReader>(day3_ticks);
    Backtester bt3("AAPL", std::move(reader3), std::move(strat3), om);
    bt3.run();
    strat3_ptr->on_day_close_with_prices(std::chrono::system_clock::from_time_t(std::mktime(&tm_day3)), {{"AAPL", 102.0}});

    // Final NAV & position
    double pos = om->get_position("AAPL");
    double cash = om->get_cash();
    double nav = cash + pos * 102.0;
    double pos_pct = (pos * 102.0) / nav;

    EXPECT_NEAR(std::abs(pos_pct), 0.0, 0.001); // <=0.1%
} 