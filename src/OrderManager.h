#pragma once

#include "IOrderManager.h"
#include <iostream>

namespace qse {

class OrderManager : public IOrderManager {
public:
    // This is now just a DECLARATION. The body is in the .cpp file.
    OrderManager(double starting_cash, double commission, double slippage);

    // These are just DECLARATIONS.
    void execute_buy(const qse::Bar& bar) override;
    void execute_sell(const qse::Bar& bar) override;
    int get_position() const override;
    double get_portfolio_value(double current_price) const override;
    double get_cash() const;

private:
    int position_;
    double cash_;
    const double commission_per_trade_;
    const double slippage_per_trade_;
};

} // namespace qse 