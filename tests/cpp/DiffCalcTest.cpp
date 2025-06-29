#include <gtest/gtest.h>
#include "qse/exe/FactorExecutionEngine.h"
#include <unordered_map>
#include <cmath>
#include "qse/data/Data.h"
#include "mocks/MockOrderManager.h"

using namespace qse;

TEST(DiffCalcTest, CashNeutral) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: NAV = $1M, weights {+0.1, -0.1} for cash-neutral portfolio
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1},   // 10% long
        {"GOOG", -0.1}   // 10% short
    };
    
    std::unordered_map<std::string, double> current_holdings = {
        {"AAPL", 0.0},   // No current position
        {"GOOG", 0.0}    // No current position
    };
    
    double cash = 1000000.0;  // $1M cash
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0},  // $150 per share
        {"GOOG", 2500.0}  // $2500 per share
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Verify we get target shares for both symbols
    EXPECT_EQ(target_shares.size(), 2);
    EXPECT_GT(target_shares["AAPL"], 0);  // Should be positive (buy)
    EXPECT_LT(target_shares["GOOG"], 0);  // Should be negative (sell)
    
    // Verify cash neutrality: ΣΔ$ ≈ 0 (< 1e-2 × NAV)
    double total_dollar_impact = 0.0;
    for (const auto& share_pair : target_shares) {
        const std::string& symbol = share_pair.first;
        long long shares = share_pair.second;
        double price = prices[symbol];
        total_dollar_impact += shares * price;
    }
    
    double tolerance = 1e-2 * cash;  // 1% of NAV
    EXPECT_NEAR(total_dollar_impact, 0.0, tolerance);
    
    // Verify specific calculations
    // AAPL: 0.1 * 1M / 150 = 666.67 shares ≈ 667 shares
    // GOOG: -0.1 * 1M / 2500 = -40 shares
    EXPECT_NEAR(target_shares["AAPL"], 667, 10);  // Allow some rounding tolerance
    EXPECT_NEAR(target_shares["GOOG"], -40, 5);
}

TEST(DiffCalcTest, ExistingPositions) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: NAV = $1M, target weights {0.2, -0.1}, existing positions
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.2},   // 20% target
        {"GOOG", -0.1}   // -10% target
    };
    
    std::unordered_map<std::string, double> current_holdings = {
        {"AAPL", 500.0},   // Already have 500 shares
        {"GOOG", -20.0}    // Already short 20 shares
    };
    
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0},
        {"GOOG", 2500.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // NAV = 1M + 500*150 + (-20)*2500 = 1M + 75K - 50K = 1.025M
    // AAPL: target = 0.2 * 1.025M / 150 = 1366.67, current = 500, delta = 866.67
    // GOOG: target = -0.1 * 1.025M / 2500 = -41, current = -20, delta = -21
    EXPECT_GT(target_shares["AAPL"], 0);   // Need to buy more
    EXPECT_LT(target_shares["GOOG"], 0);   // Need to sell more (increase short)
    
    EXPECT_NEAR(target_shares["AAPL"], 867, 10);
    EXPECT_NEAR(target_shares["GOOG"], -21, 5);
}

TEST(DiffCalcTest, LotSizeRounding) {
    ExecConfig cfg;
    cfg.lot_size = 100;  // Round to 100-share lots
    FactorExecutionEngine engine(cfg, nullptr);
    
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.15}   // 15% target
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // AAPL: 0.15 * 1M / 150 = 1000 shares
    // Should be rounded to nearest 100-share lot
    EXPECT_EQ(target_shares["AAPL"], 1000);
    EXPECT_EQ(target_shares["AAPL"] % 100, 0);  // Should be divisible by lot size
}

TEST(DiffCalcTest, MissingPriceData) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1},
        {"UNKNOWN", 0.1}  // No price data for this symbol
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0}   // Only AAPL has price data
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Should only include AAPL, skip UNKNOWN due to missing price
    EXPECT_EQ(target_shares.size(), 1);
    EXPECT_EQ(target_shares.count("AAPL"), 1);
    EXPECT_EQ(target_shares.count("UNKNOWN"), 0);
}

TEST(DiffCalcTest, ZeroPriceHandling) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1},
        {"ZERO_PRICE", 0.1}  // Zero price
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0},
        {"ZERO_PRICE", 0.0}  // Invalid price
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Should only include AAPL, skip ZERO_PRICE due to invalid price
    EXPECT_EQ(target_shares.size(), 1);
    EXPECT_EQ(target_shares.count("AAPL"), 1);
    EXPECT_EQ(target_shares.count("ZERO_PRICE"), 0);
}

TEST(DiffCalcTest, TinyWeightZeroShares) {
    ExecConfig cfg;
    cfg.lot_size = 100;  // Large lot size
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Very small weight on high-price stock
    std::unordered_map<std::string, double> target_weights = {
        {"BRK_A", 0.0001}   // 0.01% target weight
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;  // $1M cash
    
    std::unordered_map<std::string, double> prices = {
        {"BRK_A", 500000.0}  // $500K per share (Berkshire Hathaway A)
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Δw × NAV / price = 0.0001 × 1M / 500K = 0.2 shares
    // With lot_size=100, this should round to 0 shares
    EXPECT_EQ(target_shares.size(), 0);  // Should be empty (no meaningful trades)
}

TEST(DiffCalcTest, DownwardAdjustments) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Large existing position, small target weight
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.05}   // 5% target (down from current)
    };
    
    std::unordered_map<std::string, double> current_holdings = {
        {"AAPL", 1000.0}   // Already have 1000 shares
    };
    
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // NAV = 1M + 1000*150 = 1.15M
    // Target = 0.05 * 1.15M / 150 = 383.33 shares
    // Current = 1000 shares
    // Delta = 383.33 - 1000 = -616.67 shares (sell)
    EXPECT_LT(target_shares["AAPL"], 0);  // Should be negative (sell)
    EXPECT_NEAR(target_shares["AAPL"], -617, 10);
}

TEST(DiffCalcTest, NAVCalculationEdge) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Mixed long/short positions + cash
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1},   // 10% target
        {"GOOG", -0.05}  // -5% target
    };
    
    std::unordered_map<std::string, double> current_holdings = {
        {"AAPL", 500.0},   // Long 500 shares
        {"GOOG", -20.0},   // Short 20 shares
        {"TSLA", 100.0},   // Long 100 shares (not in target weights)
        {"NFLX", -50.0}    // Short 50 shares (not in target weights)
    };
    
    double cash = 500000.0;  // $500K cash
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0},
        {"GOOG", 2500.0},
        {"TSLA", 200.0},
        {"NFLX", 400.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // NAV = 500K + 500*150 + (-20)*2500 + 100*200 + (-50)*400
    // NAV = 500K + 75K - 50K + 20K - 20K = 525K
    // AAPL: target = 0.1 * 525K / 150 = 350 shares, current = 500, delta = -150
    // GOOG: target = -0.05 * 525K / 2500 = -10.5 shares, current = -20, delta = 9.5
    EXPECT_LT(target_shares["AAPL"], 0);  // Should sell AAPL
    EXPECT_GT(target_shares["GOOG"], 0);  // Should buy GOOG (reduce short)
    EXPECT_NEAR(target_shares["AAPL"], -150, 10);
    EXPECT_NEAR(target_shares["GOOG"], 10, 5);
}

TEST(DiffCalcTest, FractionalLotSizeBehavior) {
    ExecConfig cfg;
    cfg.lot_size = 1;  // Allow single shares
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Weights that produce fractional shares
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.123456}   // Fractional weight
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Target = 0.123456 * 1M / 150 = 823.04 shares
    // Should round to 823 shares (standard round() behavior)
    EXPECT_EQ(target_shares["AAPL"], 823);
}

TEST(DiffCalcTest, ExtremeNAVPriceValues) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Extreme NAV and price values
    std::unordered_map<std::string, double> target_weights = {
        {"PENNY", 0.1},   // 10% target
        {"MEGA", 0.01}    // 1% target
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000000.0;  // $1B cash
    
    std::unordered_map<std::string, double> prices = {
        {"PENNY", 0.001},  // $0.001 per share (penny stock)
        {"MEGA", 1000000.0} // $1M per share (hypothetical)
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // PENNY: 0.1 * 1B / 0.001 = 100B shares (should be skipped due to overflow risk)
    // MEGA: 0.01 * 1B / 1M = 10 shares
    EXPECT_EQ(target_shares.size(), 1);  // Should only include MEGA
    EXPECT_EQ(target_shares.count("MEGA"), 1);
    EXPECT_EQ(target_shares.count("PENNY"), 0);  // Should skip penny stock
    EXPECT_EQ(target_shares["MEGA"], 10);
}

TEST(DiffCalcTest, ToleranceTightening) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    // Setup: Cash-neutral portfolio with tighter tolerance
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1},   // 10% long
        {"GOOG", -0.1}   // 10% short
    };
    
    std::unordered_map<std::string, double> current_holdings = {};
    double cash = 1000000.0;
    
    std::unordered_map<std::string, double> prices = {
        {"AAPL", 150.0},
        {"GOOG", 2500.0}
    };
    
    auto target_shares = engine.calc_target_shares(target_weights, current_holdings, cash, prices);
    
    // Verify tighter cash neutrality: ΣΔ$ ≈ 0 (< 0.1% × NAV)
    double total_dollar_impact = 0.0;
    for (const auto& share_pair : target_shares) {
        const std::string& symbol = share_pair.first;
        long long shares = share_pair.second;
        double price = prices[symbol];
        total_dollar_impact += shares * price;
    }
    
    double tight_tolerance = 0.001 * cash;  // 0.1% of NAV (much tighter)
    EXPECT_NEAR(total_dollar_impact, 0.0, tight_tolerance);
    
    // Verify the tolerance is actually tighter than the default 1%
    double default_tolerance = 0.01 * cash;
    EXPECT_LT(tight_tolerance, default_tolerance);
}

TEST(DiffCalcTest, BuildOrdersMarketStyle) {
    ExecConfig cfg;
    cfg.order_style = "market";
    cfg.min_qty = 10;
    FactorExecutionEngine engine(cfg, nullptr);
    std::unordered_map<std::string, long long> target_qty = {
        {"AAPL", 50}, {"GOOG", 5}, {"TSLA", -20}
    };
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1}, {"GOOG", 0.05}, {"TSLA", -0.08}
    };
    auto orders = engine.build_orders(target_qty, target_weights);
    // Only AAPL and TSLA should be included (|GOOG| <= min_qty)
    ASSERT_EQ(orders.size(), 2);
    for (const auto& order : orders) {
        EXPECT_TRUE(order.type == Order::Type::MARKET);
        EXPECT_EQ(order.target_percent, 0.0);
        if (order.symbol == "AAPL") {
            EXPECT_EQ(order.side, Order::Side::BUY);
            EXPECT_EQ(order.quantity, 50);
        } else if (order.symbol == "TSLA") {
            EXPECT_EQ(order.side, Order::Side::SELL);
            EXPECT_EQ(order.quantity, 20);
        } else {
            FAIL() << "Unexpected symbol: " << order.symbol;
        }
    }
}

TEST(DiffCalcTest, BuildOrdersTargetPercentStyle) {
    ExecConfig cfg;
    cfg.order_style = "target_percent";
    cfg.min_qty = 0;
    FactorExecutionEngine engine(cfg, nullptr);
    std::unordered_map<std::string, long long> target_qty = {
        {"AAPL", 100}, {"GOOG", -50}
    };
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.12}, {"GOOG", -0.07}
    };
    auto orders = engine.build_orders(target_qty, target_weights);
    ASSERT_EQ(orders.size(), 2);
    for (const auto& order : orders) {
        EXPECT_TRUE(order.type == Order::Type::TARGET_PERCENT);
        if (order.symbol == "AAPL") {
            EXPECT_EQ(order.side, Order::Side::BUY);
            EXPECT_EQ(order.quantity, 100);
            EXPECT_NEAR(order.target_percent, 0.12, 1e-8);
        } else if (order.symbol == "GOOG") {
            EXPECT_EQ(order.side, Order::Side::SELL);
            EXPECT_EQ(order.quantity, 50);
            EXPECT_NEAR(order.target_percent, -0.07, 1e-8);
        } else {
            FAIL() << "Unexpected symbol: " << order.symbol;
        }
    }
}

TEST(DiffCalcTest, BuildOrdersMinQtyThreshold) {
    ExecConfig cfg;
    cfg.order_style = "market";
    cfg.min_qty = 100;
    FactorExecutionEngine engine(cfg, nullptr);
    std::unordered_map<std::string, long long> target_qty = {
        {"AAPL", 99}, {"GOOG", 100}, {"TSLA", -101}
    };
    std::unordered_map<std::string, double> target_weights = {
        {"AAPL", 0.1}, {"GOOG", 0.2}, {"TSLA", -0.3}
    };
    auto orders = engine.build_orders(target_qty, target_weights);
    // Only TSLA should be included (|TSLA| = 101 > 100)
    // AAPL (99) and GOOG (100) should be skipped (not strictly greater than min_qty)
    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].symbol, "TSLA");
    EXPECT_EQ(orders[0].quantity, 101);
    EXPECT_EQ(orders[0].side, Order::Side::SELL);
}

TEST(DispatcherTest, CallsOM) {
    using ::testing::_;
    using ::testing::Return;
    using ::testing::Exactly;
    auto mock_om = std::make_shared<MockOrderManager>();
    qse::ExecConfig cfg;
    cfg.order_style = "market";
    qse::FactorExecutionEngine engine(cfg, mock_om);
    std::vector<qse::Order> orders;
    // Add two market orders
    qse::Order o1; o1.symbol = "AAPL"; o1.type = qse::Order::Type::MARKET; o1.side = qse::Order::Side::BUY; o1.quantity = 100;
    qse::Order o2; o2.symbol = "GOOG"; o2.type = qse::Order::Type::MARKET; o2.side = qse::Order::Side::SELL; o2.quantity = 50;
    orders.push_back(o1); orders.push_back(o2);
    EXPECT_CALL(*mock_om, submit_market_order("AAPL", qse::Order::Side::BUY, 100)).Times(1);
    EXPECT_CALL(*mock_om, submit_market_order("GOOG", qse::Order::Side::SELL, 50)).Times(1);
    engine.submit_orders(orders);
}

TEST(DispatcherTest, TargetPercentLogsWarning) {
    using ::testing::_;
    auto mock_om = std::make_shared<MockOrderManager>();
    qse::ExecConfig cfg;
    cfg.order_style = "target_percent";
    qse::FactorExecutionEngine engine(cfg, mock_om);
    std::vector<qse::Order> orders;
    qse::Order o1; o1.symbol = "AAPL"; o1.type = qse::Order::Type::TARGET_PERCENT; o1.side = qse::Order::Side::BUY; o1.quantity = 100; o1.target_percent = 0.12;
    orders.push_back(o1);
    EXPECT_CALL(*mock_om, submit_market_order(_, _, _)).Times(0);
    engine.submit_orders(orders);
    // (Warning is printed to stderr, not checked here)
}

TEST(DispatcherTest, NoOrders_NoCalls) {
    using ::testing::_;
    auto mock_om = std::make_shared<MockOrderManager>();
    qse::FactorExecutionEngine engine({}, mock_om);
    EXPECT_CALL(*mock_om, submit_market_order(_, _, _)).Times(0);
    engine.submit_orders({});
}

TEST(DispatcherTest, ExceptionHandling_ContinuesAfterReject) {
    using ::testing::_;
    using ::testing::Throw;
    auto mock_om = std::make_shared<MockOrderManager>();
    qse::FactorExecutionEngine engine({}, mock_om);
    std::vector<qse::Order> orders;
    qse::Order o1; o1.symbol = "AAPL"; o1.type = qse::Order::Type::MARKET; o1.side = qse::Order::Side::BUY; o1.quantity = 100;
    qse::Order o2; o2.symbol = "GOOG"; o2.type = qse::Order::Type::MARKET; o2.side = qse::Order::Side::SELL; o2.quantity = 50;
    orders.push_back(o1); orders.push_back(o2);
    // First order throws, second should still be processed
    EXPECT_CALL(*mock_om, submit_market_order("AAPL", qse::Order::Side::BUY, 100))
        .WillOnce(Throw(std::runtime_error("Order rejected")));
    EXPECT_CALL(*mock_om, submit_market_order("GOOG", qse::Order::Side::SELL, 50))
        .Times(1);
    engine.submit_orders(orders);
    // (Error is printed to stderr, not checked here)
}

TEST(DispatcherTest, UnsupportedTypeExplicitError) {
    using ::testing::_;
    auto mock_om = std::make_shared<MockOrderManager>();
    qse::FactorExecutionEngine engine({}, mock_om);
    std::vector<qse::Order> orders;
    qse::Order o1; o1.symbol = "AAPL"; o1.type = qse::Order::Type::TARGET_PERCENT; o1.side = qse::Order::Side::BUY; o1.quantity = 100; o1.target_percent = 0.12;
    qse::Order o2; o2.symbol = "GOOG"; o2.type = qse::Order::Type::LIMIT; o2.side = qse::Order::Side::SELL; o2.quantity = 50; o2.limit_price = 150.0;
    orders.push_back(o1); orders.push_back(o2);
    // Neither should call submit_market_order (both unsupported types)
    EXPECT_CALL(*mock_om, submit_market_order(_, _, _)).Times(0);
    engine.submit_orders(orders);
    // (Warnings are printed to stderr, not checked here)
}

TEST(RebalanceGuardTest, OncePerDay) {
    qse::ExecConfig cfg;
    cfg.rebal_time = "15:45";
    qse::FactorExecutionEngine engine(cfg, nullptr);
    
    // Create a time point for 15:45 on a specific date
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;  // 2024
    tm.tm_mon = 0;             // January
    tm.tm_mday = 15;           // 15th
    tm.tm_hour = 15;           // 15:45
    tm.tm_min = 45;
    tm.tm_sec = 0;
    
    auto rebalance_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // First call on this date should return true
    EXPECT_TRUE(engine.should_rebalance(rebalance_time));
    
    // Second call on same date should return false
    EXPECT_FALSE(engine.should_rebalance(rebalance_time));
    
    // Call at different time (16:45) should return false
    tm.tm_hour = 16;
    auto different_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_FALSE(engine.should_rebalance(different_time));
    
    // Call on different date should return true
    tm.tm_mday = 16;  // Next day
    tm.tm_hour = 15;  // Back to 15:45
    auto next_day_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    EXPECT_TRUE(engine.should_rebalance(next_day_time));
    
    // Second call on next day should return false
    EXPECT_FALSE(engine.should_rebalance(next_day_time));
}
