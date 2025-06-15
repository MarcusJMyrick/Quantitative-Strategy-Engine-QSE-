#pragma once
#include <iostream>

namespace qse {

class OrderManager {
public:
    OrderManager() : position_(0), cash_(100000.0), pnl_(0.0) {}

    void execute_buy(double price) {
        // Simple logic: go long one unit
        position_ += 1;
        cash_ -= price;
        std::cout << "Executed BUY at " << price << std::endl;
    }

    void execute_sell(double price) {
        // Simple logic: go short one unit
        position_ -= 1;
        cash_ += price;
        std::cout << "Executed SELL at " << price << std::endl;
    }

    int get_position() const { return position_; }
    double get_cash() const { return cash_; }
    double get_pnl() const { return pnl_; }

private:
    int position_;
    double cash_;
    double pnl_; // To be implemented later
};

} // namespace qse 