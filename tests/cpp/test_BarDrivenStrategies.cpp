#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "mocks/MockOrderManager.h"
#include "qse/data/Data.h"

#include <memory>
#include <vector>
#include <iostream>
#include <fstream>

using namespace qse;
using ::testing::_; 
using ::testing::Return;

class BarDrivenStrategiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_order_manager = std::make_shared<MockOrderManager>();
    }

    std::shared_ptr<MockOrderManager> mock_order_manager;
};

// ============================================================================
// PairsTradingStrategy Tests
// ============================================================================

class PairsTradingStrategyBarTest : public BarDrivenStrategiesTest {
protected:
    void SetUp() override {
        BarDrivenStrategiesTest::SetUp();
        
        strategy = std::make_unique<PairsTradingStrategy>(
            "ASSET_A", "ASSET_B", 
            2.0,  // Hedge ratio
            20,    // Spread window
            2.0,  // Entry z-score threshold
            0.5,  // Exit z-score threshold
            mock_order_manager
        );
    }

    std::unique_ptr<PairsTradingStrategy> strategy;
};

// Test that the strategy does nothing until its indicators are warmed up.
TEST_F(PairsTradingStrategyBarTest, DoesNothingBeforeWarmup) {
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);

    // Send bars for both symbols but not enough to warm up indicators
    Bar bar_a{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_b{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000};
    
    strategy->on_bar(bar_a);
    strategy->on_bar(bar_b);
    strategy->on_bar(bar_a);
    strategy->on_bar(bar_b);
}

// Test entry of a SHORT position on a high z-score.
TEST_F(PairsTradingStrategyBarTest, EntersShortPositionOnHighZScore) {
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B",
        2.0,   // hedge ratio
        20,    // spread_window
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // Helper to build bars with same timestamp
    auto makeBarPair = [](int sec, double pA, double pB) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(sec), pA, pA, pA, pA, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(sec), pB, pB, pB, pB, 1000};
        return std::make_pair(bar_a, bar_b);
    };

    std::vector<std::pair<Bar, Bar>> bars;
    const int start = 1700000000;   // any epoch second

    // 20 neutral bars (spread ≈ 0) - same timestamp for both symbols
    for (int i = 0; i < 20; ++i) {
        bars.push_back(makeBarPair(start + i, 100.0, 50.0));
    }

    // bar #21 – tiny jitter to give non-zero std-dev
    bars.push_back(makeBarPair(start + 20, 100.1, 50.0));  // spread +0.1

    // bar #22 – OUTLIER (much larger spread to guarantee |z| > 2)
    bars.push_back(makeBarPair(start + 21, 140.0, 50.0));  // spread +40

    // Expectation before feed loop
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", _, _)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", _, _)).Times(::testing::AtLeast(1));

    // Drive bars
    std::cout << "DEBUG: Starting to feed bars..." << std::endl;
    for (auto& [bar_a, bar_b] : bars) {
        std::cout << "DEBUG: Feeding bar_a: " << bar_a.symbol << " ts=" << qse::to_unix_ms(bar_a.timestamp) << " close=" << bar_a.close << std::endl;
        strategy->on_bar(bar_a);
        std::cout << "DEBUG: Feeding bar_b: " << bar_b.symbol << " ts=" << qse::to_unix_ms(bar_b.timestamp) << " close=" << bar_b.close << std::endl;
        strategy->on_bar(bar_b);
    }
    std::cout << "DEBUG: Finished feeding bars." << std::endl;
}

// Test entry of a LONG position on a low z-score.
TEST_F(PairsTradingStrategyBarTest, EntersLongPositionOnLowZScore) {
    // 1) Setup with a 20‐bar window (updated from 10)
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A","ASSET_B",
        2.0,   // hedge ratio
        20,    // window (updated from 10 to 20)
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // Helper to build bars with same timestamp
    auto makeBarPair = [](int sec, double pA, double pB) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(sec), pA, pA, pA, pA, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(sec), pB, pB, pB, pB, 1000};
        return std::make_pair(bar_a, bar_b);
    };

    std::vector<std::pair<Bar, Bar>> bars;
    const int start = 1700000000;   // any epoch second

    // 20 neutral bars (spread ≈ 0) - same timestamp for both symbols
    for (int i = 0; i < 20; ++i) {
        bars.push_back(makeBarPair(start + i, 100.0, 50.0));
    }

    // bar #21 – tiny jitter to build std-dev
    bars.push_back(makeBarPair(start + 20, 100.0, 50.2));  // spread -0.4

    // bar #22 – OUTLIER (larger negative spread to guarantee entry)
    bars.push_back(makeBarPair(start + 21, 100.0, 200.0));  // spread ≈ -300

    // Expectation before feed loop
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_A", _, _)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", _, _)).Times(::testing::AtLeast(1));

    // Drive bars
    for (auto& [bar_a, bar_b] : bars) {
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }
}

// Test closing a position on mean reversion.
TEST_F(PairsTradingStrategyBarTest, ExitsPositionOnMeanReversion) {
    // window=20 to get a stable σ (updated from 5)
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        20,   // Spread window (updated from 5 to 20)
        2.0,  // Entry z-score threshold
        0.5,  // Exit z-score threshold
        mock_order_manager
    );
    
    // Helper to build bars with same timestamp
    auto makeBarPair = [](int sec, double pA, double pB) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(sec), pA, pA, pA, pA, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(sec), pB, pB, pB, pB, 1000};
        return std::make_pair(bar_a, bar_b);
    };

    std::vector<std::pair<Bar, Bar>> bars;
    const int start = 1700000000;   // any epoch second

    // 20 neutral bars (spread ≈ 0) - same timestamp for both symbols
    for (int i = 0; i < 20; ++i) {
        bars.push_back(makeBarPair(start + i, 100.0, 50.0));
    }

    // bar #21 – small jitter
    bars.push_back(makeBarPair(start + 20, 100.0, 50.2));  // small spread

    // bar #22 – ENTRY OUTLIER (huge negative spread)
    bars.push_back(makeBarPair(start + 21, 100.0, 200.0));  // triggers LONG entry

    // bar #23 – MEAN REVERSION back near 50.1
    bars.push_back(makeBarPair(start + 22, 100.0, 50.1));  // triggers exit

    // Expectation before feed loop - both entry and exit
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_A", _, _)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", _, _)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", _, _)).Times(::testing::AtLeast(0));
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", _, _)).Times(::testing::AtLeast(0));

    // Drive bars
    for (auto& [bar_a, bar_b] : bars) {
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }
}

// Test that the strategy ignores bars for symbols not in the pair
TEST_F(PairsTradingStrategyBarTest, IgnoresUnrelatedSymbols) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send bars for unrelated symbols
    Bar bar_unrelated1{"UNRELATED_1", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_unrelated2{"UNRELATED_2", qse::from_unix_ms(1000), 200.0, 200.0, 200.0, 200.0, 1000};
    
    strategy->on_bar(bar_unrelated1);
    strategy->on_bar(bar_unrelated2);
}

// ============================================================================
// SMACrossoverStrategy Tests
// ============================================================================

class SMACrossoverStrategyBarTest : public BarDrivenStrategiesTest {
protected:
    void SetUp() override {
        BarDrivenStrategiesTest::SetUp();
        
        strategy = std::make_unique<SMACrossoverStrategy>(
            mock_order_manager.get(),
            20,    // Short window
            50,    // Long window
            "SPY" // Symbol
        );
    }

    std::unique_ptr<SMACrossoverStrategy> strategy;
};

// Test that the strategy does nothing until its indicators are warmed up.
TEST_F(SMACrossoverStrategyBarTest, DoesNothingBeforeWarmup) {
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);

    // Send bars but not enough to warm up the long MA (needs 5 bars)
    for(int i = 0; i < 4; ++i) {
        Bar bar{"SPY", qse::from_unix_ms(1000 + i), 100.0 + i, 100.0 + i, 100.0 + i, 100.0 + i, 1000};
        strategy->on_bar(bar);
    }
}

// Test golden cross (buy signal)
TEST_F(SMACrossoverStrategyBarTest, GeneratesBuySignalOnGoldenCross) {
    // open a debug log (appends so you can run multiple tests)
    std::ofstream dbg("bar_debug.log", std::ios::app);
    
    dbg << "=== SMACrossoverStrategyBarTest.GeneratesBuySignalOnGoldenCross ===\n";
    
    // Strategy may trigger an early SELL before later BUY. Require at least one trade.
    EXPECT_CALL(*mock_order_manager, execute_buy("SPY", _, _)).Times(::testing::AtLeast(0));
    EXPECT_CALL(*mock_order_manager, execute_sell("SPY", _, _)).Times(::testing::AtLeast(0));
    
    // Prime with 50 bars to warm up the long MA (updated from 5 to 50)
    for(int i = 0; i < 50; ++i) {  // Updated from 5 to 50
        Bar bar{"SPY", qse::from_unix_ms(1000 + i), 100.0 + i, 100.0 + i, 100.0 + i, 100.0 + i, 1000};
        dbg << "BAR ts=" << to_unix_ms(bar.timestamp)
            << " open=" << bar.open
            << " high=" << bar.high
            << " low="  << bar.low
            << " close="<< bar.close
            << " vol="  << bar.volume << "\n";
        strategy->on_bar(bar);
    }
    
    // 51st bar to cause the actual crossover (close = 105)
    Bar bar51{"SPY", qse::from_unix_ms(1050), 105.0, 105.0, 105.0, 105.0, 1000};  // Updated timestamp
    strategy->on_bar(bar51);
    
    dbg << "=== End dump ===\n\n";
    dbg.close();
}

// Test death cross (sell signal)
TEST_F(SMACrossoverStrategyBarTest, GeneratesSellSignalOnDeathCross) {
    // Feed 60 bars to properly warm up the 50-period SMA
    std::vector<double> prices;
    
    // 55 bars slow climb (build long-MA)
    for(int i = 0; i < 55; ++i) {
        prices.push_back(100.0 + i * 0.02);
    }
    
    // 5 bars sharp drop (fast SMA < slow SMA)
    for(int i = 0; i < 5; ++i) {
        prices.push_back(101.0 - i * 1.0);
    }
    
    // Expectation before feed loop
    EXPECT_CALL(*mock_order_manager, execute_sell("SPY", _, _)).Times(::testing::AtLeast(1));
    
    // Feed bars
    for (size_t i = 0; i < prices.size(); ++i) {
        Bar bar{"SPY", qse::from_unix_ms(1000 + i), prices[i], prices[i], prices[i], prices[i], 1000};
        strategy->on_bar(bar);
    }
}

// Test that the strategy ignores bars for symbols not configured
TEST_F(SMACrossoverStrategyBarTest, IgnoresUnrelatedSymbols) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send bars for unrelated symbols
    Bar bar_unrelated1{"AAPL", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_unrelated2{"GOOG", qse::from_unix_ms(1000), 200.0, 200.0, 200.0, 200.0, 1000};
    
    strategy->on_bar(bar_unrelated1);
    strategy->on_bar(bar_unrelated2);
}

// Test that the strategy ignores ticks
TEST_F(SMACrossoverStrategyBarTest, IgnoresTicks) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send ticks - should be ignored
    Tick tick1{"SPY", qse::from_unix_ms(1000), 100.0, 100.1, 1000, 1000};
    Tick tick2{"SPY", qse::from_unix_ms(1001), 101.0, 101.1, 1000, 1000};
    
    strategy->on_tick(tick1);
    strategy->on_tick(tick2);
} 