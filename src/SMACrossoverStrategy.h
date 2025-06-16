#pragma once

#include "IStrategy.h"
#include "IOrderManager.h"
#include <deque>

namespace qse {

class SMACrossoverStrategy : public IStrategy {
public:
    SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window);
    void on_bar(const Bar& bar) override;

private:
    IOrderManager* order_manager_;
    size_t short_window_;
    size_t long_window_;
    std::deque<double> prices_;
    double prev_short_sma_ = 0.0;
    double prev_long_sma_ = 0.0;
    double short_sum_ = 0.0;
    double long_sum_ = 0.0;
};

} // namespace qse 