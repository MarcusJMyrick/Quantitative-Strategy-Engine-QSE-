#include "SMACrossoverStrategy.h"
#include <iostream>

namespace qse {

SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window)
    : order_manager_(order_manager),
      short_ma_(short_window),
      long_ma_(long_window) {
    if (short_window >= long_window) {
        throw std::invalid_argument("Short window must be smaller than long window.");
    }
    std::cout << "Initialized SMA Crossover Strategy with windows: short=" 
              << short_window << ", long=" << long_window << std::endl;
}

void SMACrossoverStrategy::on_bar(const qse::Bar& bar) {
    // Store previous values before updating
    double prev_short_val = short_ma_.get_value();
    double prev_long_val = long_ma_.get_value();

    // Update both moving averages with the new price
    short_ma_.update(bar.close);
    long_ma_.update(bar.close);

    // Wait until both moving averages have enough data
    if (!short_ma_.is_ready() || !long_ma_.is_ready()) {
        return;
    }
    
    double current_short_val = short_ma_.get_value();
    double current_long_val = long_ma_.get_value();

    // --- Crossover Logic ---
    // Golden Cross (Buy Signal)
    if (prev_short_val < prev_long_val && current_short_val > current_long_val) {
        order_manager_->execute_buy(bar);
    }
    // Death Cross (Sell Signal)
    else if (prev_short_val > prev_long_val && current_short_val < current_long_val) {
        order_manager_->execute_sell(bar);
    }
}

} // namespace qse 