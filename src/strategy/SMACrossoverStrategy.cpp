#include "qse/strategy/SMACrossoverStrategy.h"
#include <iostream>

namespace qse {

// The constructor now also initializes the BarBuilder.
// For this example, we'll build 1-day bars.
SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window, std::chrono::minutes bar_duration)
    : order_manager_(order_manager),
      short_ma_(short_window),
      long_ma_(long_window),
      bar_builder_(bar_duration) // <-- Pass the duration to the BarBuilder
{}

// This is now the primary entry point for new data.
// It receives every single tick from the backtester.
void SMACrossoverStrategy::on_tick(const qse::Tick& tick) {
    // Feed the tick to our internal BarBuilder
    if (auto bar = bar_builder_.add_tick(tick)) {
        // If a bar was completed, process it
        on_bar(*bar);
    }
}

// This function contains the original crossover logic. It's now called by on_tick
// only when a full bar has been constructed.
void SMACrossoverStrategy::on_bar(const qse::Bar& bar) {
    double prev_short_val = short_ma_.get_value();
    double prev_long_val = long_ma_.get_value();

    short_ma_.update(bar.close);
    long_ma_.update(bar.close);

    if (!long_ma_.is_ready()) {
        return;
    }
    
    double current_short_val = short_ma_.get_value();
    double current_long_val = long_ma_.get_value();

    if (prev_long_val > 0) { // Guard against trading on the very first calculation
        // Golden Cross (Buy Signal)
        if (prev_short_val < prev_long_val && current_short_val > current_long_val) {
            // Use the new OrderManager interface with symbol, quantity, and price
            order_manager_->execute_buy(bar.symbol, 1, bar.close);
        }
        // Death Cross (Sell Signal)
        else if (prev_short_val > prev_long_val && current_short_val < current_long_val) {
            // Use the new OrderManager interface with symbol, quantity, and price
            order_manager_->execute_sell(bar.symbol, 1, bar.close);
        }
    }
}

} // namespace qse