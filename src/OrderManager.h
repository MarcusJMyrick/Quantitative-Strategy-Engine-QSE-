#pragma once
#include <iostream>
#include "IOrderManager.h"
#include <vector>

namespace qse {

class OrderManager : public IOrderManager {
public:
    OrderManager() : position_(0), cash_(100000.0) {}

    void execute_buy(double price) override {
        position_ += 1;
        cash_ -= price;
        std::cout << "Executed BUY at " << price << std::endl;
    }

    void execute_sell(double price) override {
        position_ -= 1;
        cash_ += price;
        std::cout << "Executed SELL at " << price << std::endl;
    }

    int get_position() const { return position_; }
    
    double get_portfolio_value(double current_price) const {
        return cash_ + (position_ * current_price);
    }

private:
    int position_;
    double cash_;
    double pnl_; // To be implemented later
    std::vector<double> trade_prices_;
};

} // namespace qse 