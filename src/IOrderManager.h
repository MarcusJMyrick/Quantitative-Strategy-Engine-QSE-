// In src/IOrderManager.h

#pragma once
#include "Data.h"
#include <vector> // <-- Make sure <vector> is included

namespace qse {

class IOrderManager {
public:
    virtual ~IOrderManager() = default;
    virtual void execute_buy(const qse::Bar& bar) = 0;
    virtual void execute_sell(const qse::Bar& bar) = 0;
    virtual int get_position() const = 0;
    virtual double get_portfolio_value(double current_price) const = 0;

    // ---> ADD THIS LINE <---
    virtual const std::vector<Trade>& get_trade_log() const = 0;
};

} // namespace qse