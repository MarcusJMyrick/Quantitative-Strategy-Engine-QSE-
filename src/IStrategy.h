#pragma once
#include "Data.h"
#include <vector>

namespace qse {

// An abstract interface for all trading strategies.
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // The engine will call this method for each new bar of market data.
    virtual void on_bar(const Bar& bar) = 0;
};

} // namespace qse 