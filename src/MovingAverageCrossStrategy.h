#pragma once
#include "IStrategy.h"

class MovingAverageCrossStrategy : public IStrategy {
public:
    MovingAverageCrossStrategy(int short_window, int long_window);
    std::vector<Order> generateSignals(const std::vector<MarketData>& data) override;

private:
    int short_window_;
    int long_window_;
}; 