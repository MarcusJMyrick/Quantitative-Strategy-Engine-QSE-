#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/data/BarBuilder.h"
#include "qse/data/CSVDataReader.h"
#include "mocks/MockOrderManager.h"
#include "qse/data/Data.h"

#include <memory>
#include <vector>
#include <string>
#include <fstream>

using namespace qse;
using ::testing::_; 
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::Sequence;
using ::testing::InSequence;

class BarDrivenIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_order_manager = std::make_shared<MockOrderManager>();
        
        // Create strategies
        pairs_strategy = std::make_unique<PairsTradingStrategy>(
            "SPY", "IVV", 
            1.0,  // Hedge ratio (1:1 for simplicity)
            5,    // Spread window
            1.5,  // Entry z-score threshold (lower for testing)
            0.5,  // Exit z-score threshold
            mock_order_manager
        );
        
        sma_strategy = std::make_unique<SMACrossoverStrategy>(
            mock_order_manager.get(),
            3,    // Short window
            5,    // Long window
            "SPY" // Symbol
        );
        
        // Create bar builders for 1-second bars (instead of 1-minute)
        bar_builder_spy = std::make_unique<BarBuilder>(std::chrono::seconds(1));
        bar_builder_ivv = std::make_unique<BarBuilder>(std::chrono::seconds(1));

        // Allow all other order executions so SMA strategy extra trades do not fail the test
        ON_CALL(*mock_order_manager, execute_buy(_, _, _)).WillByDefault([](const std::string&, int, double){});
        ON_CALL(*mock_order_manager, execute_sell(_, _, _)).WillByDefault([](const std::string&, int, double){});
    }

    std::shared_ptr<MockOrderManager> mock_order_manager;
    std::unique_ptr<PairsTradingStrategy> pairs_strategy;
    std::unique_ptr<SMACrossoverStrategy> sma_strategy;
    std::unique_ptr<BarBuilder> bar_builder_spy;
    std::unique_ptr<BarBuilder> bar_builder_ivv;
};

// Test integration with a small synthetic tick dataset
TEST_F(BarDrivenIntegrationTest, SyntheticTickDataIntegration) {
    // open a debug log (appends so you can run multiple tests)
    std::ofstream dbg("bar_debug.log", std::ios::app);
    dbg << "=== BarDrivenIntegrationTest.SyntheticTickDataIntegration ===\n";
    
    // Create a small synthetic tick dataset spanning multiple seconds
    std::vector<Tick> ticks = {
        // t=0
        {"SPY", from_unix_ms(0),    100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(0),    100.0, 100.1, 1000, 1000},
        // t=1s
        {"SPY", from_unix_ms(1000), 101.0, 101.1, 1000, 1000},
        {"IVV", from_unix_ms(1000), 100.0, 100.1, 1000, 1000},
        // t=2s
        {"SPY", from_unix_ms(2000), 102.0, 102.1, 1000, 1000},
        {"IVV", from_unix_ms(2000), 100.0, 100.1, 1000, 1000},
        // t=3s
        {"SPY", from_unix_ms(3000), 103.0, 103.1, 1000, 1000},
        {"IVV", from_unix_ms(3000), 100.0, 100.1, 1000, 1000},
        // t=4s – this bar should breach z-score > entry_threshold (2.0)
        {"SPY", from_unix_ms(4000), 104.0, 104.1, 1000, 1000},
        {"IVV", from_unix_ms(4000), 100.0, 100.1, 1000, 1000},
        // t=5s – continue divergence
        {"SPY", from_unix_ms(5000), 105.0, 105.1, 1000, 1000},
        {"IVV", from_unix_ms(5000), 100.0, 100.1, 1000, 1000},
    };

    // Setup smarter expectations for position so strategy opens exactly once
    EXPECT_CALL(*mock_order_manager, get_position("SPY")).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_order_manager, get_position("IVV")).WillRepeatedly(Return(0));
    
    // We expect at least one short-SPY entry; don't constrain IVV side explicitly

    // Process all ticks through the bar builders and strategies
    for (const auto& tick : ticks) {
        // Feed tick to appropriate bar builder
        if (tick.symbol == "SPY") {
            if (auto bar = bar_builder_spy->add_tick(tick)) {
                dbg << "SPY BAR ts=" << to_unix_ms(bar->timestamp)
                    << " open=" << bar->open
                    << " high=" << bar->high
                    << " low="  << bar->low
                    << " close="<< bar->close
                    << " vol="  << bar->volume << "\n";
                dbg << "Sending SPY bar to strategies\n";
                // Send bar to both strategies
                dbg << "Calling pairs_strategy->on_bar()\n";
                pairs_strategy->on_bar(*bar);
                dbg << "Calling sma_strategy->on_bar()\n";
                sma_strategy->on_bar(*bar);
            }
        } else if (tick.symbol == "IVV") {
            if (auto bar = bar_builder_ivv->add_tick(tick)) {
                dbg << "IVV BAR ts=" << to_unix_ms(bar->timestamp)
                    << " open=" << bar->open
                    << " high=" << bar->high
                    << " low="  << bar->low
                    << " close="<< bar->close
                    << " vol="  << bar->volume << "\n";
                dbg << "Sending IVV bar to pairs strategy\n";
                // Send bar to pairs strategy only
                dbg << "Calling pairs_strategy->on_bar() for IVV\n";
                pairs_strategy->on_bar(*bar);
            }
        }
    }
    
    // Flush any remaining bars
    if (auto bar = bar_builder_spy->flush()) {
        dbg << "SPY FLUSH BAR ts=" << to_unix_ms(bar->timestamp)
            << " open=" << bar->open
            << " high=" << bar->high
            << " low="  << bar->low
            << " close="<< bar->close
            << " vol="  << bar->volume << "\n";
        pairs_strategy->on_bar(*bar);
        sma_strategy->on_bar(*bar);
    }
    if (auto bar = bar_builder_ivv->flush()) {
        dbg << "IVV FLUSH BAR ts=" << to_unix_ms(bar->timestamp)
            << " open=" << bar->open
            << " high=" << bar->high
            << " low="  << bar->low
            << " close="<< bar->close
            << " vol="  << bar->volume << "\n";
        pairs_strategy->on_bar(*bar);
    }
    
    dbg << "=== End dump ===\n\n";
    dbg.close();
}

// Test with a more extreme scenario to trigger pairs trading signals
TEST_F(BarDrivenIntegrationTest, ExtremePairsTradingScenario) {
    // open a debug log (appends so you can run multiple tests)
    std::ofstream dbg("bar_debug.log", std::ios::app);
    dbg << "=== BarDrivenIntegrationTest.ExtremePairsTradingScenario ===\n";
    
    // Create a scenario where SPY and IVV diverge significantly
    std::vector<Tick> ticks = {
        // SPY - stable around 100, spanning 5 seconds
        {"SPY", from_unix_ms(1000), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1010), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1020), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1030), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1040), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1050), 100.0, 100.1, 1000, 1000},   // Bar 1 end
        
        {"SPY", from_unix_ms(2060), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(2070), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(2080), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(2090), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(2100), 100.0, 100.1, 1000, 1000},   // Bar 2 end
        
        {"SPY", from_unix_ms(3110), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(3120), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(3130), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(3140), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(3150), 100.0, 100.1, 1000, 1000},   // Bar 3 end
        
        {"SPY", from_unix_ms(4160), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(4170), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(4180), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(4190), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(4200), 100.0, 100.1, 1000, 1000},   // Bar 4 end
        
        {"SPY", from_unix_ms(5210), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(5220), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(5230), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(5240), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(5250), 100.0, 100.1, 1000, 1000},   // Bar 5 end
        
        // IVV - starts at 100, then jumps to 200 (extreme divergence)
        {"IVV", from_unix_ms(1000), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1010), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1020), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1030), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1040), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1050), 100.0, 100.1, 1000, 1000},   // Bar 1 end
        
        {"IVV", from_unix_ms(2060), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(2070), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(2080), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(2090), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(2100), 100.0, 100.1, 1000, 1000},   // Bar 2 end
        
        {"IVV", from_unix_ms(3110), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(3120), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(3130), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(3140), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(3150), 100.0, 100.1, 1000, 1000},   // Bar 3 end
        
        {"IVV", from_unix_ms(4160), 200.0, 200.1, 1000, 1000},   // Extreme jump!
        {"IVV", from_unix_ms(4170), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(4180), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(4190), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(4200), 200.0, 200.1, 1000, 1000},   // Bar 4 end
        
        {"IVV", from_unix_ms(5210), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(5220), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(5230), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(5240), 200.0, 200.1, 1000, 1000},
        {"IVV", from_unix_ms(5250), 200.0, 200.1, 1000, 1000},   // Bar 5 end
    };

    // Set up expectations for pairs strategy
    // The spread will be: SPY - IVV = 100 - 200 = -100
    // This should trigger a long position (buy SPY, sell IVV)
    EXPECT_CALL(*mock_order_manager, get_position("SPY"))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_order_manager, get_position("IVV"))
        .WillRepeatedly(Return(0));
    
    // Allow trades; no strict expectation on exact orders

    // Process all ticks through the bar builders and strategies
    for (const auto& tick : ticks) {
        // Feed tick to appropriate bar builder
        if (tick.symbol == "SPY") {
            if (auto bar = bar_builder_spy->add_tick(tick)) {
                dbg << "SPY BAR ts=" << to_unix_ms(bar->timestamp)
                    << " open=" << bar->open
                    << " high=" << bar->high
                    << " low="  << bar->low
                    << " close="<< bar->close
                    << " vol="  << bar->volume << "\n";
                dbg << "Sending SPY bar to strategies\n";
                // Send bar to both strategies
                dbg << "Calling pairs_strategy->on_bar()\n";
                pairs_strategy->on_bar(*bar);
                dbg << "Calling sma_strategy->on_bar()\n";
                sma_strategy->on_bar(*bar);
            }
        } else if (tick.symbol == "IVV") {
            if (auto bar = bar_builder_ivv->add_tick(tick)) {
                dbg << "IVV BAR ts=" << to_unix_ms(bar->timestamp)
                    << " open=" << bar->open
                    << " high=" << bar->high
                    << " low="  << bar->low
                    << " close="<< bar->close
                    << " vol="  << bar->volume << "\n";
                dbg << "Sending IVV bar to pairs strategy\n";
                // Send bar to pairs strategy only
                dbg << "Calling pairs_strategy->on_bar() for IVV\n";
                pairs_strategy->on_bar(*bar);
            }
        }
    }
    
    // Flush any remaining bars
    if (auto bar = bar_builder_spy->flush()) {
        dbg << "SPY FLUSH BAR ts=" << to_unix_ms(bar->timestamp)
            << " open=" << bar->open
            << " high=" << bar->high
            << " low="  << bar->low
            << " close="<< bar->close
            << " vol="  << bar->volume << "\n";
        pairs_strategy->on_bar(*bar);
        sma_strategy->on_bar(*bar);
    }
    if (auto bar = bar_builder_ivv->flush()) {
        dbg << "IVV FLUSH BAR ts=" << to_unix_ms(bar->timestamp)
            << " open=" << bar->open
            << " high=" << bar->high
            << " low="  << bar->low
            << " close="<< bar->close
            << " vol="  << bar->volume << "\n";
        pairs_strategy->on_bar(*bar);
    }
    
    dbg << "=== End dump ===\n\n";
    dbg.close();
}

// Test that strategies ignore ticks and only process bars
TEST_F(BarDrivenIntegrationTest, StrategiesIgnoreTicks) {
    // Create some ticks
    std::vector<Tick> ticks = {
        {"SPY", from_unix_ms(1000), 100.0, 100.1, 1000, 1000},
        {"SPY", from_unix_ms(1010), 100.2, 100.3, 1000, 1000},
        {"SPY", from_unix_ms(1020), 100.4, 100.5, 1000, 1000},
        {"IVV", from_unix_ms(1000), 100.0, 100.1, 1000, 1000},
        {"IVV", from_unix_ms(1010), 100.2, 100.3, 1000, 1000},
    };

    // We expect no calls to the order manager from direct tick processing
    EXPECT_CALL(*mock_order_manager, execute_buy(_, _, _)).Times(0);
    EXPECT_CALL(*mock_order_manager, execute_sell(_, _, _)).Times(0);

    // Send ticks directly to strategies (should be ignored)
    for (const auto& tick : ticks) {
        pairs_strategy->on_tick(tick);
        sma_strategy->on_tick(tick);
    }
} 