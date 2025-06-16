#pragma once

#include "IStrategy.h"
#include "IOrderManager.h"
#include "MovingAverage.h" // The helper class does all the work

namespace qse {

class SMACrossoverStrategy : public IStrategy {
public:
    SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window);

    void on_bar(const qse::Bar& bar) override;

private:
    IOrderManager* order_manager_;
    
    // The strategy now ONLY holds two MovingAverage objects.
    // They manage all the state (prices, sums, etc.) internally.
    MovingAverage short_ma_;
    MovingAverage long_ma_;
};

} // namespace qse