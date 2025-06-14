#pragma once
#include "Data.h"
#include <vector>

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual std::vector<Order> generateSignals(const std::vector<MarketData>& data) = 0;
}; 