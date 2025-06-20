#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/order/IOrderManager.h"
#include "qse/strategy/MovingAverage.h"
#include "qse/data/BarBuilder.h" // <-- Include the new BarBuilder
#include <memory>

namespace qse {

class SMACrossoverStrategy : public qse::IStrategy {
public:
    SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window);

    // Implement the new on_tick method from the interface.
    void on_tick(const qse::Tick& tick) override;

    // The on_bar method remains, but its role changes slightly.
    void on_bar(const qse::Bar& bar) override;

private:
    IOrderManager* order_manager_;
    MovingAverage short_ma_;
    MovingAverage long_ma_;
    
    // --- NEW: The strategy now owns a BarBuilder ---
    // It will use this to construct bars from the incoming tick stream.
    BarBuilder bar_builder_;
};

} // namespace qse