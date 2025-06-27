#include "qse/strategy/SMACrossoverStrategy.h"
#include <iostream>
#include <fstream>

namespace qse {

// Constructor - no longer needs bar duration since we're bar-driven
SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window, const std::string& symbol)
    : order_manager_(order_manager),
      short_ma_(short_window),
      long_ma_(long_window),
      symbol_(symbol)
{}

// Ignore all ticks - this strategy is bar-driven
void SMACrossoverStrategy::on_tick(const qse::Tick& tick) {
    // Do nothing - we only process bars
}

// Main strategy logic - process bars for our symbol
void SMACrossoverStrategy::on_bar(const qse::Bar& bar) {
    if (bar.symbol != symbol_) {
        return;
    }

    double prev_short_val = short_ma_.get_value();
    double prev_long_val = long_ma_.get_value();

    short_ma_.update(bar.close);
    long_ma_.update(bar.close);

    if (!long_ma_.is_ready()) {
        return;
    }

    double current_short_val = short_ma_.get_value();
    double current_long_val = long_ma_.get_value();

    std::cerr << "[SMACrossoverStrategy] " << symbol_ 
              << " | Close: " << bar.close
              << " | PrevShort: " << prev_short_val << " | PrevLong: " << prev_long_val
              << " | CurrShort: " << current_short_val << " | CurrLong: " << current_long_val << std::endl;

    if (long_ma_.is_ready()) {
        if (prev_short_val < prev_long_val && current_short_val > current_long_val) {
            std::cerr << "[SMACrossoverStrategy] GOLDEN CROSS for " << symbol_ << "! Submitting BUY order." << std::endl;
            order_manager_->execute_buy(symbol_, 1, bar.close);
        } else if (prev_short_val > prev_long_val && current_short_val < current_long_val) {
            std::cerr << "[SMACrossoverStrategy] DEATH CROSS for " << symbol_ << "! Submitting SELL order." << std::endl;
            order_manager_->execute_sell(symbol_, 1, bar.close);
        }
    }
}

} // namespace qse