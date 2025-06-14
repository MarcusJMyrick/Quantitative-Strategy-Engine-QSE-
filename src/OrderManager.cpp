#include "OrderManager.h"

namespace qse {

OrderManager::OrderManager(double starting_cash, double commission, double slippage)
    : position_(0),
      cash_(starting_cash),
      commission_per_trade_(commission),
      slippage_per_trade_(slippage) {}

void OrderManager::execute_buy(const qse::Bar& bar) {
    // Slippage: The price moves against us, so we buy at a slightly higher price.
    double execution_price = bar.close + slippage_per_trade_;
    position_ += 1;
    // We pay for the asset and also pay a commission.
    cash_ -= (execution_price + commission_per_trade_);
    std::cout << "Executed BUY at " << execution_price 
              << " (Close: " << bar.close << ", Costs: " 
              << slippage_per_trade_ + commission_per_trade_ << ")" << std::endl;
}

void OrderManager::execute_sell(const qse::Bar& bar) {
    // Slippage: The price moves against us, so we sell at a slightly lower price.
    double execution_price = bar.close - slippage_per_trade_;
    position_ -= 1;
    // We receive cash for the asset but must pay a commission.
    cash_ += (execution_price - commission_per_trade_);
    std::cout << "Executed SELL at " << execution_price 
              << " (Close: " << bar.close << ", Costs: " 
              << slippage_per_trade_ + commission_per_trade_ << ")" << std::endl;
}

int OrderManager::get_position() const {
    return position_;
}

double OrderManager::get_portfolio_value(double current_price) const {
    return cash_ + (position_ * current_price);
}

double OrderManager::get_cash() const {
    return cash_;
}

} // namespace qse 