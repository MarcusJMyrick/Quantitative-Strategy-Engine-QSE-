#include "SMACrossoverStrategy.h"
#include <iostream>

namespace qse {

SMACrossoverStrategy::SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window)
    : order_manager_(order_manager),
      short_ma_(short_window),
      long_ma_(long_window)
{
    if (short_window >= long_window) {
        throw std::invalid_argument("Short window must be smaller than long window.");
    }
}

void SMACrossoverStrategy::on_bar(const qse::Bar& bar) {
    // First, get the value of the moving averages BEFORE the new bar.
    double prev_short_val = short_ma_.get_value();
    double prev_long_val = long_ma_.get_value();

    // Now, update both moving averages with the new closing price.
    short_ma_.update(bar.close);
    long_ma_.update(bar.close);

    // Wait until the long window (the larger one) is ready.
    if (!long_ma_.is_ready()) {
        return;
    }
    
    // Get the new, updated values.
    double current_short_val = short_ma_.get_value();
    double current_long_val = long_ma_.get_value();

    // --- Crossover Logic ---
    // A crossover only happens if the previous values were not yet calculated (i.e., were 0)
    // or if the relationship between them has flipped.
    if (prev_long_val > 0) { // Guard against trading on the very first calculation
        // Golden Cross (Buy Signal): was below, is now above.
        if (prev_short_val < prev_long_val && current_short_val > current_long_val) {
            order_manager_->execute_buy(bar);
        }
        // Death Cross (Sell Signal): was above, is now below.
        else if (prev_short_val > prev_long_val && current_short_val < current_long_val) {
            order_manager_->execute_sell(bar);
        }
    }
}

} // namespace qse