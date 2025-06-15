#pragma once

#include "IStrategy.h"
#include "OrderManager.h"
#include <vector>
#include <deque>

namespace qse {

class SMACrossoverStrategy : public IStrategy {
public:
    SMACrossoverStrategy(OrderManager* order_manager, size_t short_window, size_t long_window);

    void on_bar(const Bar& bar) override;

private:
    double calculate_sma(const std::deque<double>& data);

    OrderManager* order_manager_;
    const size_t short_window_;
    const size_t long_window_;

    std::deque<double> prices_;
    
    // Variables to store the previous state to detect a cross
    double prev_short_sma_ = 0.0;
    double prev_long_sma_ = 0.0;
};

} // namespace qse 