#pragma once
#include "Data.h"

// The interface for all trading strategies.
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // --- NEW: Called for every single tick ---
    // This will become the primary way the engine feeds data to the strategy.
    virtual void on_tick(const qse::Tick& tick) = 0;

    // Called when a new bar is formed (can be used by lower-frequency strategies).
    // This will now typically be called by the strategy's own BarBuilder.
    virtual void on_bar(const qse::Bar& bar) = 0;
};
