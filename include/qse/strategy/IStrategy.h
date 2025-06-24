#pragma once
#include "qse/data/Data.h"

namespace qse {

// The interface for all trading strategies.
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // --- NEW: Called for every single tick ---
    // This will become the primary way the engine feeds data to the strategy.
    // Default implementation does nothing - strategies can override if needed.
    virtual void on_tick(const qse::Tick& tick) {}

    // Called when a new bar is formed (can be used by lower-frequency strategies).
    // This will now typically be called by the strategy's own BarBuilder.
    // Default implementation does nothing - strategies can override if needed.
    virtual void on_bar(const qse::Bar& bar) {}
};

} // namespace qse
