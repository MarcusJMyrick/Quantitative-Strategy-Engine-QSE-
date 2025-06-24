#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/data/Data.h"

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
    
    // Override on_tick to do nothing (but we could add logging here)
    void on_tick(const Tick& tick) override {
        // Do nothing - this is intentional for smoke testing
    }
    
    // Override on_bar to do nothing (but we could add logging here)
    void on_bar(const Bar& bar) override {
        // Do nothing - this is intentional for smoke testing
    }
};

} // namespace qse 