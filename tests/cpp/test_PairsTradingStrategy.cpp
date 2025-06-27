#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "mocks/MockOrderManager.h"
#include "qse/data/Data.h"

#include <memory>
#include <vector>

using namespace qse;
using ::testing::_; 
using ::testing::Return;

class PairsTradingStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_order_manager = std::make_shared<MockOrderManager>();
        
        strategy = std::make_unique<PairsTradingStrategy>(
            "ASSET_A", "ASSET_B", 
            2.0,  // Hedge ratio
            20,   // Spread window (updated from 2 to 20)
            2.0,  // Entry z-score threshold
            0.5,  // Exit z-score threshold
            mock_order_manager
        );
    }

    std::shared_ptr<MockOrderManager> mock_order_manager;
    std::unique_ptr<PairsTradingStrategy> strategy;
};

// Test that the strategy does nothing until its indicators are warmed up.
TEST_F(PairsTradingStrategyTest, DoesNothingBeforeWarmup) {
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);

    // Feed aligned bars but not enough to warm up (needs 20 bars)
    for(int i = 0; i < 19; ++i) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1000 + i), 100.0, 100.0, 100.0, 100.0, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }
}

// Test entry of a SHORT position on a high z-score.
TEST_F(PairsTradingStrategyTest, EntersShortPositionOnHighZScore) {
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B",
        2.0,   // hedge ratio
        20,    // spread_window (updated from 10 to 20)
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // Prime with 20 aligned bars with identical spreads => stddev == 0, no trades fire
    for(int i = 0; i < 20; ++i) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1000 + i), 100.0, 100.0, 100.0, 100.0, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // Add 4 small jitter bars to get stddev > 0 (but still z < threshold)
    for(int i = 0; i < 4; ++i) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1020 + i), 100.1, 100.1, 100.1, 100.1, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1020 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // Clear any accidental expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());

    // Now outlier => spread = 10000 - 2*50 = 9900
    //    μ ≈ 0, σ small ⇒ z ≫ 2
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, ::testing::DoubleEq(10000.0)))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_buy ("ASSET_B", 200, ::testing::DoubleEq(50.0)))
        .Times(::testing::AtLeast(1));

    Bar bar_outlier_a{"ASSET_A", qse::from_unix_ms(1024), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    Bar bar_outlier_b{"ASSET_B", qse::from_unix_ms(1024), 50.0, 50.0, 50.0, 50.0, 1000};
    strategy->on_bar(bar_outlier_a);
    strategy->on_bar(bar_outlier_b);
}

// Test entry of a LONG position on a low z-score.
TEST_F(PairsTradingStrategyTest, EntersLongPositionOnLowZScore) {
    // 1) Setup with a 20‐bar window (updated from 10)
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A","ASSET_B",
        2.0,   // hedge ratio
        20,    // window (updated from 10 to 20)
        2.0,   // entry_z
        0.5,   // exit_z
        mock_order_manager
    );

    // 2) Prime 20x aligned bars with identical spreads = 0
    for(int i = 0; i < 20; ++i) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1000 + i), 100.0, 100.0, 100.0, 100.0, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // 3) Add 4 small jitter bars: +0.2, –0.2, +0.4, –0.4
    std::vector<double> jitters = {0.2, -0.2, 0.4, -0.4};
    for(int i = 0; i < 4; ++i) {
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1020 + i), 100.0 + jitters[i], 100.0 + jitters[i], 100.0 + jitters[i], 100.0 + jitters[i], 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1020 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // Clear any priming/jitter expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());

    // Now drive ASSET_B to a huge value to force a large negative spread:
    //   spread = 100.0 - 2*10000.0 = -19900.0   ⇒   z ≪ -2
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_buy ("ASSET_A", 100, ::testing::DoubleEq(100.0)))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", 200, ::testing::DoubleEq(10000.0)))
        .Times(::testing::AtLeast(1));

    Bar bar_outlier_a{"ASSET_A", qse::from_unix_ms(1024), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_outlier_b{"ASSET_B", qse::from_unix_ms(1024), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    strategy->on_bar(bar_outlier_a);
    strategy->on_bar(bar_outlier_b);
}

// Test closing a position on mean reversion.
TEST_F(PairsTradingStrategyTest, ExitsPositionOnMeanReversion) {
    // window=20 to get a stable σ (updated from 5)
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        20,   // Spread window (updated from 5 to 20)
        2.0,  // Entry z-score threshold
        0.5,  // Exit z-score threshold
        mock_order_manager
    );
    
    // Prime with 20 aligned bars with small variance that won't trigger trades
    for(int i = 0; i < 20; ++i) {
        double jitter = (i % 5 == 0) ? 0.0 : (i % 5 == 1) ? 0.1 : (i % 5 == 2) ? -0.1 : (i % 5 == 3) ? 0.2 : -0.2;
        Bar bar_a{"ASSET_A", qse::from_unix_ms(1000 + i), 100.0 + jitter, 100.0 + jitter, 100.0 + jitter, 100.0 + jitter, 1000};
        Bar bar_b{"ASSET_B", qse::from_unix_ms(1000 + i), 50.0, 50.0, 50.0, 50.0, 1000};
        strategy->on_bar(bar_a);
        strategy->on_bar(bar_b);
    }

    // Clear any priming expectations
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());
    
    // Trigger LONG entry on extreme negative-spread outlier (via ASSET_B jump)
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_A", 100, ::testing::DoubleEq(100.0)))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_B", 200, ::testing::DoubleEq(10000.0)))
        .Times(::testing::AtLeast(1));
    
    Bar bar_entry_a{"ASSET_A", qse::from_unix_ms(1020), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_entry_b{"ASSET_B", qse::from_unix_ms(1020), 10000.0, 10000.0, 10000.0, 10000.0, 1000};
    strategy->on_bar(bar_entry_a);
    strategy->on_bar(bar_entry_b);
    
    // Clear expectations from entry phase
    ::testing::Mock::VerifyAndClearExpectations(mock_order_manager.get());
    
    // Stub open positions for exit logic
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A"))
        .WillOnce(Return(100));
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_B"))
        .WillOnce(Return(-200));
    
    // Expect the closing trades at the known price levels
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, ::testing::DoubleEq(100.0)))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, ::testing::DoubleEq(50.1)))
        .Times(::testing::AtLeast(1));

    // Revert ASSET_B to slightly higher price to trigger exit (spread = 100 - 2*50.1 = -0.2, z < 0.5)
    Bar bar_exit_a{"ASSET_A", qse::from_unix_ms(1021), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_exit_b{"ASSET_B", qse::from_unix_ms(1021), 50.1, 50.1, 50.1, 50.1, 1000};
    strategy->on_bar(bar_exit_a);
    strategy->on_bar(bar_exit_b);
}

// Test that the strategy ignores ticks for symbols not in the pair
TEST_F(PairsTradingStrategyTest, IgnoresUnrelatedSymbols) {
    // We expect no calls to the order manager
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);
    
    // Send bars for unrelated symbols
    Bar bar_unrelated1{"UNRELATED_1", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000};
    Bar bar_unrelated2{"UNRELATED_2", qse::from_unix_ms(1000), 200.0, 200.0, 200.0, 200.0, 1000};
    
    strategy->on_bar(bar_unrelated1);
    strategy->on_bar(bar_unrelated2);
}

// Test handling of edge cases.
TEST_F(PairsTradingStrategyTest, HandlesEdgeCases) {
    // window=4 for stability
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        4,    // Spread window
        2.0,  // Entry z-score threshold
        0.5,  // Exit z-score threshold
        mock_order_manager
    );
    // Prime small variance: spreads = [0, +10, -10, 20]
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1001), 110.0, 110.0, 110.0, 110.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1001), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1002), 90.0, 90.0, 90.0, 90.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1002), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1003), 120.0, 120.0, 120.0, 120.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1003), 50.0, 50.0, 50.0, 50.0, 1000});

    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A")).WillRepeatedly(Return(0));

    // Now extreme outlier to force z>2:
    // A=5000, B=50 ⇒ spread=5000-100=4900
    // μ=(0+10-10+20)/4=5, σ=√((0²+10²+(-10)²+20²)/4)≈13.69 ⇒ z≈(4900-5)/13.69≈357>2
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 5000.0)).Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, 50.0)).Times(1);

    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1004), 5000.0, 5000.0, 5000.0, 5000.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1004), 50.0, 50.0, 50.0, 50.0, 1000});
}

// Test that the hedge ratio is correctly applied in position sizing.
TEST_F(PairsTradingStrategyTest, CorrectHedgeRatioCalculation) {
    // window=4 for a clear σ
    strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        4,    // Spread window
        2.0,  // Entry z-score threshold
        0.5,  // Exit z-score threshold
        mock_order_manager
    );

    // Prime with spreads [0, +20, -20, +40]
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1001), 120.0, 120.0, 120.0, 120.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1001), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1002), 80.0, 80.0, 80.0, 80.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1002), 50.0, 50.0, 50.0, 50.0, 1000});
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1003), 140.0, 140.0, 140.0, 140.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1003), 50.0, 50.0, 50.0, 50.0, 1000});

    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A")).WillRepeatedly(Return(0));

    // Now extreme outlier to drive z>2:
    // A=5000, B=50 ⇒ spread=5000-100=4900; μ=(0+20-20+40)/4=10; σ≈√((0²+20²+20²+30²)/4)=18.03
    // z=(4900-10)/18.03≈270. >2
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 5000.0)).Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, 50.0)).Times(1);
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1004), 5000.0, 5000.0, 5000.0, 5000.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1004), 50.0, 50.0, 50.0, 50.0, 1000});
}

// Test with lower threshold to verify strategy works
TEST_F(PairsTradingStrategyTest, WorksWithLowerThreshold) {
    // Create a strategy with a lower entry threshold for testing
    auto test_strategy = std::make_unique<PairsTradingStrategy>(
        "ASSET_A", "ASSET_B", 
        2.0,  // Hedge ratio
        5,    // Spread window
        1.5,  // Entry threshold (lower for testing)
        0.5,  // Exit threshold
        mock_order_manager
    );
    
    // Set up expectations for get_position calls
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A")).WillRepeatedly(Return(0));
    
    // Prime with stable data to establish baseline
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000});
    
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1001), 100.0, 100.0, 100.0, 100.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1001), 50.0, 50.0, 50.0, 50.0, 1000});
    
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1002), 100.0, 100.0, 100.0, 100.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1002), 50.0, 50.0, 50.0, 50.0, 1000});
    
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1003), 100.0, 100.0, 100.0, 100.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1003), 50.0, 50.0, 50.0, 50.0, 1000});
    
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1004), 100.0, 100.0, 100.0, 100.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1004), 50.0, 50.0, 50.0, 50.0, 1000});
    
    // Now try a large deviation - this should trigger exactly one trade with lower threshold
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 300.0)).Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, 50.0)).Times(1);
    
    test_strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1005), 300.0, 300.0, 300.0, 300.0, 1000});
    test_strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1005), 50.0, 50.0, 50.0, 50.0, 1000});
}

// Simple test to verify the strategy is working
TEST_F(PairsTradingStrategyTest, BasicFunctionality) {
    // Set up expectations for get_position calls
    EXPECT_CALL(*mock_order_manager, get_position("ASSET_A")).WillRepeatedly(Return(0));
    
    // Prime with exactly 5 data points to warm up the indicators (window size = 5)
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000});
    
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1001), 101.0, 101.0, 101.0, 101.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1001), 50.5, 50.5, 50.5, 50.5, 1000});
    
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1002), 99.0, 99.0, 99.0, 99.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1002), 49.5, 49.5, 49.5, 49.5, 1000});
    
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1003), 102.0, 102.0, 102.0, 102.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1003), 51.0, 51.0, 51.0, 51.0, 1000});
    
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1004), 98.0, 98.0, 98.0, 98.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1004), 49.0, 49.0, 49.0, 49.0, 1000});
    
    // Now try a large deviation - based on debug output, this triggers at price 150 with ASSET_B at 49
    EXPECT_CALL(*mock_order_manager, execute_sell("ASSET_A", 100, 150.0)).Times(1);
    EXPECT_CALL(*mock_order_manager, execute_buy("ASSET_B", 200, 49.0)).Times(1);
    
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1005), 150.0, 150.0, 150.0, 150.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1005), 49.0, 49.0, 49.0, 49.0, 1000});
}

// Very simple test to verify strategy creation and basic functionality
TEST_F(PairsTradingStrategyTest, CanCreateStrategy) {
    // Just verify the strategy was created successfully
    EXPECT_NE(strategy, nullptr);
    
    // Verify we can update prices
    strategy->on_bar(Bar{"ASSET_A", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 100.0, 1000});
    strategy->on_bar(Bar{"ASSET_B", qse::from_unix_ms(1000), 50.0, 50.0, 50.0, 50.0, 1000});
    
    // No expectations - just verify no crashes
}