#include "SMACrossoverStrategy.h"
#include <numeric>
#include <iostream>
#include <iomanip>

namespace qse {

SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window)
    : order_manager_(order_manager), short_window_(short_window), long_window_(long_window) {
    if (short_window >= long_window) {
        throw std::invalid_argument("Short window must be smaller than long window.");
    }
    std::cout << "Initialized SMA Crossover Strategy with windows: short=" << short_window 
              << ", long=" << long_window << std::endl;
}

void SMACrossoverStrategy::on_bar(const Bar& bar) {
    // Add the new closing price to our history
    prices_.push_back(bar.close);

    // If we have more prices than our longest window, remove the oldest one
    if (prices_.size() > long_window_) {
        prices_.pop_front();
    }

    // Wait until we have enough data for the long window
    if (prices_.size() < long_window_) {
        std::cout << "Waiting for more data... Current size: " << prices_.size() 
                  << " (need " << long_window_ << ")" << std::endl;
        return;
    }

    // Calculate current SMAs
    double current_short_sma = calculate_sma({prices_.end() - short_window_, prices_.end()});
    double current_long_sma = calculate_sma(prices_);

    // Log the current state
    std::cout << std::fixed << std::setprecision(2)
              << "Bar: " << bar.timestamp.time_since_epoch().count() 
              << " | Price: " << bar.close
              << " | Short SMA: " << current_short_sma
              << " | Long SMA: " << current_long_sma
              << " | Diff: " << (current_short_sma - current_long_sma) << std::endl;

    // --- Crossover Logic ---
    // Golden Cross (Buy Signal): Short SMA crosses above Long SMA
    if (prev_short_sma_ < prev_long_sma_ && current_short_sma > current_long_sma) {
        std::cout << "GOLDEN CROSS DETECTED! Executing BUY at " << bar.close << std::endl;
        order_manager_->execute_buy(bar.close);
    }
    // Death Cross (Sell Signal): Short SMA crosses below Long SMA
    else if (prev_short_sma_ > prev_long_sma_ && current_short_sma < current_long_sma) {
        std::cout << "DEATH CROSS DETECTED! Executing SELL at " << bar.close << std::endl;
        order_manager_->execute_sell(bar.close);
    }

    // Update previous SMA values for the next bar
    prev_short_sma_ = current_short_sma;
    prev_long_sma_ = current_long_sma;
}

double SMACrossoverStrategy::calculate_sma(const std::deque<double>& data) {
    if (data.empty()) {
        return 0.0;
    }
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

} // namespace qse 