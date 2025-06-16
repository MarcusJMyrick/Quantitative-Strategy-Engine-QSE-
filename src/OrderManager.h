#pragma once

#include "IOrderManager.h"
#include <iostream>
#include <vector> // <-- Make sure this is included
#include "Data.h" // <-- Make sure this is included for the 'Trade' struct

namespace qse {

class OrderManager : public IOrderManager {
public:
    OrderManager(double starting_cash, double commission, double slippage);

    void execute_buy(const qse::Bar& bar) override;
    void execute_sell(const qse::Bar& bar) override;
    int get_position() const override;
    double get_portfolio_value(double current_price) const override;
    
    // This is the declaration for the new function
    const std::vector<Trade>& get_trade_log() const override;

    double get_cash() const; // Helper for testing

private:
    int position_;
    double cash_;
    const double commission_per_trade_;
    const double slippage_per_trade_;
    
    // This is the new vector to store the trades
    std::vector<Trade> trade_log_;
};

} // namespace qse