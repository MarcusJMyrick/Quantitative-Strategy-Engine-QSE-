#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/data/Data.h"
#include <iostream>

namespace qse {

/**
 * @brief A simple strategy that does nothing - used for smoke testing
 * 
 * This strategy implements the IStrategy interface but performs no trading.
 * It's useful for testing the tick-driven architecture without any trading logic.
 */
class DoNothingStrategy : public IStrategy {
public:
    DoNothingStrategy() = default;
    ~DoNothingStrategy() override = default;
    
    // Override on_tick to log that we received it (for verification)
    void on_tick(const qse::Tick& tick) override {
        tick_count_++;
        if (tick_count_ % 100 == 0) { // Log every 100th tick to avoid spam
            std::cout << "[DoNothingStrategy] Received tick #" << tick_count_ 
                      << " for " << tick.symbol << " at $" << tick.price << std::endl;
        }
    }
    
    // Override on_bar to log that we received it (for verification)
    void on_bar(const qse::Bar& bar) override {
        bar_count_++;
        std::cout << "[DoNothingStrategy] Received bar #" << bar_count_ 
                  << " OHLC: $" << bar.open << "/" << bar.high << "/" 
                  << bar.low << "/" << bar.close << " Vol: " << bar.volume << std::endl;
    }

    // Override on_fill to log that we received it (for verification)
    void on_fill(const qse::Fill& fill) override {
        fill_count_++;
        std::cout << "[DoNothingStrategy] Received fill #" << fill_count_ 
                  << " for " << fill.symbol << " " << fill.side << " " 
                  << fill.quantity << " @ $" << fill.price << std::endl;
    }

    // Getters for testing
    int get_tick_count() const { return tick_count_; }
    int get_bar_count() const { return bar_count_; }
    int get_fill_count() const { return fill_count_; }

private:
    int tick_count_ = 0;
    int bar_count_ = 0;
    int fill_count_ = 0;
};

} // namespace qse 