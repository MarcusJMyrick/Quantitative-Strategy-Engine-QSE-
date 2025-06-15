#pragma once

#include "IStrategy.h"
#include "IOrderManager.h"
#include "MovingAverage.h"
#include <vector>
#include <deque>

namespace qse {

class SMACrossoverStrategy : public IStrategy {
public:
    SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window);

    void on_bar(const qse::Bar& bar) override;

private:
    double calculate_sma(const std::deque<double>& data);

    IOrderManager* order_manager_;
    
    // The strategy now just holds two MovingAverage objects
    MovingAverage short_ma_;
    MovingAverage long_ma_;

    std::deque<double> prices_;
    
    // Variables to store the previous state to detect a cross
    double prev_short_sma_ = 0.0;
    double prev_long_sma_ = 0.0;
};

} // namespace qse 