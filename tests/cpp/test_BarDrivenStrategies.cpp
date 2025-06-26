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
            2,    // Spread window (guaranteed for this test)
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
    Bar bar_a{"ASSET_A", from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_b{"ASSET_B", from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000};
    
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
        10,    // spread_window
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // Prime with identical spreads => stddev == 0, no trades fire
    for(int i = 0; i < 10; ++i) {
        Bar bar_a{"ASSET_A", from_unix_ms(1000 + i), 100.0, 100.0, 100.0, 100.0, 1000};
        Bar bar_b{"ASSET_B", from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // Seed a tiny jitter to get stddev > 0 (but still z < threshold)
    Bar bar_a_jitter{"ASSET_A", from_unix_ms(1010), 100.1, 100.1, 100.1, 100.1, 1000};
    Bar bar_b_jitter{"ASSET_B", from_unix_ms(1010), 50.0, 50.0, 50.0, 50.0, 1000};
    strategy->on_bar(bar_a_jitter);
    strategy->on_bar(bar_b_jitter);

    // Clear any accidental expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());

    // Reset ASSET_A back to a known positive price (so guard passes)
    Bar bar_a_reset{"ASSET_A", from_unix_ms(1011), 100.0, 100.0, 100.0, 100.0, 1000};
    strategy->on_bar(bar_a_reset);

    // Now outlier => spread = 10000 - 2*50 = 9900
    //    μ ≈ 0, σ small ⇒ z ≫ 2
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 10000.0))
        .Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy ("ASSET_B", 200,    50.0))
        .Times(1);

    Bar bar_a_outlier{"ASSET_A", from_unix_ms(1012), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    strategy->on_bar(bar_a_outlier);
}

// Test entry of a LONG position on a low z-score.
TEST_F(PairsTradingStrategyBarTest, EntersLongPositionOnLowZScore) {
    // 1) Setup with a 10‐bar window
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A","ASSET_B",
        2.0,   // hedge ratio
        10,    // window
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // 2) Prime 6x identical spreads = 0
    for(int i = 0; i < 6; ++i) {
        Bar bar_a{"ASSET_A", from_unix_ms(1000 + i), 100.0, 100.0, 100.0, 100.0, 1000};
        Bar bar_b{"ASSET_B", from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // 3) Add 4 small jitters: +0.2, –0.2, +0.4, –0.4
    Bar bar_a1{"ASSET_A", from_unix_ms(1006), 100.2, 100.2, 100.2, 100.2, 1000}; // +0.2
    Bar bar_a2{"ASSET_A", from_unix_ms(1007), 99.8, 99.8, 99.8, 99.8, 1000};   // –0.2
    Bar bar_a3{"ASSET_A", from_unix_ms(1008), 100.4, 100.4, 100.4, 100.4, 1000}; // +0.4
    Bar bar_a4{"ASSET_A", from_unix_ms(1009), 99.6, 99.6, 99.6, 99.6, 1000};   // –0.4
    Bar bar_b_jitter{"ASSET_B", from_unix_ms(1006), 50.0, 50.0, 50.0, 50.0, 1000};
    
    strategy->on_bar(bar_a1); strategy->on_bar(bar_b_jitter);
    strategy->on_bar(bar_a2); strategy->on_bar(bar_b_jitter);
    strategy->on_bar(bar_a3); strategy->on_bar(bar_b_jitter);
    strategy->on_bar(bar_a4); strategy->on_bar(bar_b_jitter);

    // Now σ ≈ sqrt((.2²+.2²+.4²+.4²)/10) = sqrt(0.4/10) ≈ 0.2

    // 4) A tiny negative jitter: spread ≈ –0.01 ⇒ z≈–0.05 (no trade)
    Bar bar_a_small{"ASSET_A", from_unix_ms(1010), 99.99, 99.99, 99.99, 99.99, 1000};
    strategy->on_bar(bar_a_small);
    strategy->on_bar(bar_b_jitter);

    // Clear any priming/jitter expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());

    // Reset ASSET_A back to a known positive price (so guard passes)
    Bar bar_a_reset{"ASSET_A", from_unix_ms(1011), 100.0, 100.0, 100.0, 100.0, 1000};
    strategy->on_bar(bar_a_reset);

    // Now drive ASSET_B to a huge value to force a large negative spread:
    //   spread = 100.0 - 2*10000.0 = -19900.0   ⇒   z ≪ -2
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_buy ("ASSET_A", 100,    100.0))
        .Times(1);
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", 200, 10000.0))
        .Times(1);

    Bar bar_b_outlier{"ASSET_B", from_unix_ms(1012), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    strategy->on_bar(bar_b_outlier);
}

// Test closing a position on mean reversion.
TEST_F(PairsTradingStrategyBarTest, ExitsPositionOnMeanReversion) {
    // window=5 to get a stable σ
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        5,    // Spread window
        2.0,  // Entry z-score threshold
        0.5,  // Exit z-score threshold
        mock_order_manager
    );
    
    // Prime with small variance that won't trigger trades: spreads = [0, 0.1, -0.1, 0.2, -0.2]
    Bar bar_a1{"ASSET_A", from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000}; // spread = 0
    Bar bar_a2{"ASSET_A", from_unix_ms(1001), 100.1, 100.1, 100.1, 100.1, 1000}; // spread = 0.1
    Bar bar_a3{"ASSET_A", from_unix_ms(1002), 99.9, 99.9, 99.9, 99.9, 1000};   // spread = -0.1
    Bar bar_a4{"ASSET_A", from_unix_ms(1003), 100.2, 100.2, 100.2, 100.2, 1000}; // spread = 0.2
    Bar bar_a5{"ASSET_A", from_unix_ms(1004), 99.8, 99.8, 99.8, 99.8, 1000};   // spread = -0.2
    Bar bar_b_prime{"ASSET_B", from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000};
    
    strategy->on_bar(bar_a1); strategy->on_bar(bar_b_prime);
    strategy->on_bar(bar_a2); strategy->on_bar(bar_b_prime);
    strategy->on_bar(bar_a3); strategy->on_bar(bar_b_prime);
    strategy->on_bar(bar_a4); strategy->on_bar(bar_b_prime);
    strategy->on_bar(bar_a5); strategy->on_bar(bar_b_prime);

    // Clear any priming expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());

    // Revert ASSET_A to known positive price so entry can run
    Bar bar_a_reset{"ASSET_A", from_unix_ms(1005), 100.0, 100.0, 100.0, 100.0, 1000};
    strategy->on_bar(bar_a_reset);
    
    // Trigger LONG entry on extreme negative-spread outlier (via ASSET_B jump)
    // μ = (0 + 0.1 - 0.1 + 0.2 - 0.2)/5 = 0; σ = √((0²+0.1²+0.1²+0.2²+0.2²)/5)≈0.141
    // spread = 100 - 2*10000 = -19900; z=(-19900-0)/0.141 ≈ -141134 < -2
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_A", 100, 100.0))
        .Times(1);
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", 200, 10000.0))
        .Times(1);
    
    Bar bar_b_outlier{"ASSET_B", from_unix_ms(1006), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    strategy->on_bar(bar_b_outlier);
    
    // Clear expectations from entry phase
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());
    
    // Stub open positions for exit logic
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(100));
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_B"))
        .WillOnce(Return(-200));
    
    // Expect the closing trades at the known price levels
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 100.0))
        .Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, 50.1))
        .Times(1);

    // Revert ASSET_B to slightly higher price to trigger exit (spread = 100 - 2*50.1 = -0.2, z < 0.5)
    Bar bar_b_exit{"ASSET_B", from_unix_ms(1007), 50.1, 50.1, 50.1, 50.1, 1000};
    strategy->on_bar(bar_b_exit);
}

// Test that the strategy ignores bars for symbols not in the pair
TEST_F(PairsTradingStrategyBarTest, IgnoresUnrelatedSymbols) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send bars for unrelated symbols
    Bar bar_unrelated1{"UNRELATED_1", from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_unrelated2{"UNRELATED_2", from_unix_ms(1000), 200.0, 200.0, 200.0, 200.0, 1000};
    
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
            3,    // Short window
            5,    // Long window
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
        Bar bar{"SPY", from_unix_ms(1000 + i), 100.0 + i, 100.0 + i, 100.0 + i, 100.0 + i, 1000};
        strategy->on_bar(bar);
    }
}

// Test golden cross (buy signal)
TEST_F(SMACrossoverStrategyBarTest, GeneratesBuySignalOnGoldenCross) {
    // open a debug log (appends so you can run multiple tests)
    std::ofstream dbg("bar_debug.log", std::ios::app);
    
    dbg << "=== SMACrossoverStrategyBarTest.GeneratesBuySignalOnGoldenCross ===\n";
    
    // Set up expectation for the golden cross that will happen on the 5th bar
    EXPECT_CALL(*mock_order_manager, execute_buy("SPY", 1, 104))
        .Times(1);
    
    // Prime with 5 bars to warm up the long MA
    for(int i = 0; i < 5; ++i) {
        Bar bar{"SPY", from_unix_ms(1000 + i), 100.0 + i, 100.0 + i, 100.0 + i, 100.0 + i, 1000};
        dbg << "BAR ts=" << to_unix_ms(bar.timestamp)
            << " open=" << bar.open
            << " high=" << bar.high
            << " low="  << bar.low
            << " close="<< bar.close
            << " vol="  << bar.volume << "\n";
        strategy->on_bar(bar);
    }
    
    dbg << "=== End dump ===\n\n";
    dbg.close();
}

// Test death cross (sell signal)
TEST_F(SMACrossoverStrategyBarTest, GeneratesSellSignalOnDeathCross) {
    // Prime with 5 bars to warm up the long MA
    for(int i = 0; i < 5; ++i) {
        Bar bar{"SPY", from_unix_ms(1000 + i), 100.0 + i, 100.0 + i, 100.0 + i, 100.0 + i, 1000};
        strategy->on_bar(bar);
    }
    
    // Clear any priming expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());
    
    // Create a death cross scenario:
    // Previous state: short MA > long MA
    // Add low bars to make short MA cross below long MA
    
    Bar bar_death1{"SPY", from_unix_ms(1005), 95.0, 95.0, 95.0, 95.0, 1000};
    Bar bar_death2{"SPY", from_unix_ms(1006), 93.0, 93.0, 93.0, 93.0, 1000};
    
    // New short MA = [103, 104, 95] = 100.67
    // New long MA = [102, 103, 104, 95, 93] = 99.4
    // Short MA (100.67) > Long MA (99.4) - still no cross
    
    // Add another low bar
    Bar bar_death3{"SPY", from_unix_ms(1007), 90.0, 90.0, 90.0, 90.0, 1000};
    
    // New short MA = [104, 95, 93] = 94.0
    // New long MA = [103, 104, 95, 93, 90] = 97.0
    // Short MA (94.0) < Long MA (97.0) - DEATH CROSS!
    
    EXPECT_CALL(*mock_order_manager, execute_sell("SPY", 1, 90.0))
        .Times(1);
    
    strategy->on_bar(bar_death3);
}

// Test that the strategy ignores bars for symbols not configured
TEST_F(SMACrossoverStrategyBarTest, IgnoresUnrelatedSymbols) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send bars for unrelated symbols
    Bar bar_unrelated1{"AAPL", from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_unrelated2{"GOOG", from_unix_ms(1000), 200.0, 200.0, 200.0, 200.0, 1000};
    
    strategy->on_bar(bar_unrelated1);
    strategy->on_bar(bar_unrelated2);
}

// Test that the strategy ignores ticks
TEST_F(SMACrossoverStrategyBarTest, IgnoresTicks) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send ticks - should be ignored
    Tick tick1{"SPY", from_unix_ms(1000), 100.0, 100.1, 1000, 1000};
    Tick tick2{"SPY", from_unix_ms(1001), 101.0, 101.1, 1000, 1000};
    
    strategy->on_tick(tick1);
    strategy->on_tick(tick2);
} 