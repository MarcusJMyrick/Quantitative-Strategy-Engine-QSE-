#include "SMACrossoverStrategy.h"
#include <iostream>

namespace qse {

SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window)
    : order_manager_(order_manager)
    , short_window_(short_window)
    , long_window_(long_window) {
    std::cout << "Initialized SMA Crossover Strategy with windows: short=" << short_window 
              << ", long=" << long_window << std::endl;
}

void SMACrossoverStrategy::on_bar(const Bar& bar) {
    prices_.push_back(bar.close);
    
    // Add the new price to both sums
    short_sum_ += bar.close;
    long_sum_ += bar.close;

    // --- O(1) Sum Adjustment ---
    // If the queue is now larger than a window, subtract the oldest element
    // that just fell out of that window's view.
    if (prices_.size() > long_window_) {
        long_sum_ -= prices_.front(); // Subtract oldest from long sum
    }
    if (prices_.size() > short_window_) {
        // The element that fell out of the short window is at this specific index
        short_sum_ -= prices_[prices_.size() - short_window_ - 1];
    }
    
    // Prune the main price history queue
    if (prices_.size() > long_window_) {
        prices_.pop_front();
    }

    // Wait until we have enough data
    if (prices_.size() < long_window_) {
        return;
    }

    // --- O(1) SMA Calculation ---
    double current_short_sma = short_sum_ / short_window_;
    double current_long_sma = long_sum_ / long_window_;

    // --- Crossover Logic ---
    if (prev_short_sma_ > 0 && prev_short_sma_ < prev_long_sma_ && current_short_sma > current_long_sma) {
        order_manager_->execute_buy(bar);
    }
    else if (prev_short_sma_ > 0 && prev_short_sma_ > prev_long_sma_ && current_short_sma < current_long_sma) {
        order_manager_->execute_sell(bar);
    }

    // Update previous SMA values for the next bar
    prev_short_sma_ = current_short_sma;
    prev_long_sma_ = current_long_sma;
}

} // namespace qse 