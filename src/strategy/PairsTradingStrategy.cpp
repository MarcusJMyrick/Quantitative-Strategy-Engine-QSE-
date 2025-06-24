#include "qse/strategy/PairsTradingStrategy.h"

#include <iostream>
#include <cmath>

namespace qse {

    // Constructor implementation.
    PairsTradingStrategy::PairsTradingStrategy(
        const std::string& symbol1,
        const std::string& symbol2,
        double hedge_ratio,
        int spread_window,
        double entry_threshold,
        double exit_threshold,
        std::shared_ptr<IOrderManager> order_manager)
        : symbol1_(symbol1),
          symbol2_(symbol2),
          hedge_ratio_(hedge_ratio),
          entry_threshold_(entry_threshold),
          exit_threshold_(exit_threshold),
          order_manager_(order_manager),
          spread_mean_(spread_window),
          spread_std_dev_(spread_window)
    {
        latest_prices_[symbol1_] = -1.0;
        latest_prices_[symbol2_] = -1.0;
    }

    // This strategy is tick-driven, so on_bar is empty.
    void PairsTradingStrategy::on_bar(const Bar& bar) {}

    // --- NEW: Implement on_tick to make this strategy tick-driven ---
    void PairsTradingStrategy::on_tick(const Tick& tick) {
        // Only process ticks for our symbols
        if (tick.symbol == symbol1_ || tick.symbol == symbol2_) {
            // Update the price for this symbol using the mid price
            double mid_price = (tick.bid + tick.ask) / 2.0;
            update_price(tick.symbol, mid_price);
        }
    }

    // Logic for calculating the spread and executing trades.
    void PairsTradingStrategy::check_and_execute_trades() {
#ifdef DEBUG
        std::cout << "DEBUG: Entering check_and_execute_trades(), "
                  << "spread_ready=" << spread_mean_.is_ready() << "\n";
#endif
        
        // 1. Calculate the current spread.
        double current_spread = latest_prices_[symbol1_] - (hedge_ratio_ * latest_prices_[symbol2_]);

        // 2. If indicators are not ready, just update them and wait for the next tick.
        if (!spread_mean_.is_ready()) {
#ifdef DEBUG
            std::cout << "DEBUG: Warmup – inserted spread=" << current_spread << "\n";
#endif
            spread_mean_.update(current_spread);
            spread_std_dev_.update(current_spread);
            return;
        }

        // 3. IMPORTANT: Get the historical mean and std dev BEFORE the current spread is included.
        double mean = spread_mean_.get_value();
        double std_dev = spread_std_dev_.get_value();

        // 4. NOW, update the indicators with the current spread for the *next* calculation.
        spread_mean_.update(current_spread);
        spread_std_dev_.update(current_spread);
        
        // 5. Calculate the z-score of the current spread against the historical data.
        if (std::abs(std_dev) < 1e-7) {
#ifdef DEBUG
            std::cout << "DEBUG: std_dev too small ("
                      << std_dev << "), skipping z-score calc\n";
#endif
            return; // Avoid division by zero if history is flat.
        }
        double z_score = (current_spread - mean) / std_dev;
#ifdef DEBUG
        std::cout << "DEBUG: spread=" << current_spread << ", mean=" << mean << ", stddev=" << std_dev << ", z=" << z_score << std::endl;
#endif

        // 6. Apply trading rules.
        int position_s1 = order_manager_->get_position(symbol1_);
        
        if (position_s1 == 0) {
            if (z_score > entry_threshold_) {
#ifdef DEBUG
                std::cout << "DEBUG: Branch=SHORT_ENTRY, z_score=" << z_score << "\n";
#endif
                int qty1 = 100;
                int qty2 = static_cast<int>(qty1 * hedge_ratio_);
                order_manager_->execute_sell(symbol1_, qty1, latest_prices_[symbol1_]);
                order_manager_->execute_buy(symbol2_, qty2, latest_prices_[symbol2_]);
            } else if (z_score < -entry_threshold_) {
#ifdef DEBUG
                std::cout << "DEBUG: Branch=LONG_ENTRY, z_score=" << z_score << "\n";
#endif
                int qty1 = 100;
                int qty2 = static_cast<int>(qty1 * hedge_ratio_);
                order_manager_->execute_buy(symbol1_, qty1, latest_prices_[symbol1_]);
                order_manager_->execute_sell(symbol2_, qty2, latest_prices_[symbol2_]);
            } else {
#ifdef DEBUG
                std::cout << "DEBUG: Branch=NO_ENTRY, z_score=" << z_score << " (threshold=" << entry_threshold_ << ")\n";
#endif
            }
        } else if (std::abs(z_score) < exit_threshold_) {
#ifdef DEBUG
            std::cout << "DEBUG: Branch=EXIT, z_score=" << z_score << "\n";
#endif
            int position_s2 = order_manager_->get_position(symbol2_);
            if (position_s1 > 0) {
                order_manager_->execute_sell(symbol1_, std::abs(position_s1), latest_prices_[symbol1_]);
                order_manager_->execute_buy(symbol2_, std::abs(position_s2), latest_prices_[symbol2_]);
            } else {
                order_manager_->execute_buy(symbol1_, std::abs(position_s1), latest_prices_[symbol1_]);
                order_manager_->execute_sell(symbol2_, std::abs(position_s2), latest_prices_[symbol2_]);
            }
        } else {
#ifdef DEBUG
            std::cout << "DEBUG: Branch=HOLD, z_score=" << z_score << " (exit_threshold=" << exit_threshold_ << ")\n";
#endif
        }
    }

    // Add a method to update prices for a specific symbol
    void PairsTradingStrategy::update_price(const std::string& symbol, double price) {
        if (symbol == symbol1_ || symbol == symbol2_) {
            latest_prices_[symbol] = price;
        } else {
            return;
        }
        if (latest_prices_[symbol1_] > 0 && latest_prices_[symbol2_] > 0) {
            check_and_execute_trades();
        }
    }

} // namespace qse 