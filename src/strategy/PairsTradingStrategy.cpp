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

    // Remove on_tick logic; use update_price instead
    void PairsTradingStrategy::on_tick(const Tick& tick) {}

    // Logic for calculating the spread and executing trades.
    void PairsTradingStrategy::check_and_execute_trades() {
        // 1. Calculate the current spread.
        double current_spread = latest_prices_[symbol1_] - (hedge_ratio_ * latest_prices_[symbol2_]);

        // 2. If indicators are not ready, just update them and wait for the next tick.
        if (!spread_mean_.is_ready()) {
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
            return; // Avoid division by zero if history is flat.
        }
        double z_score = (current_spread - mean) / std_dev;
        std::cout << "DEBUG: current_spread=" << current_spread << ", mean=" << mean << ", std_dev=" << std_dev << ", z_score=" << z_score << std::endl;

        // 6. Apply trading rules.
        int position_s1 = order_manager_->get_position(symbol1_);
        
        if (position_s1 == 0) {
            if (z_score > entry_threshold_) {
                std::cout << "DEBUG: Executing SHORT trade at price " << latest_prices_[symbol1_] << std::endl;
                int qty1 = 100;
                int qty2 = static_cast<int>(qty1 * hedge_ratio_);
                order_manager_->execute_sell(symbol1_, qty1, latest_prices_[symbol1_]);
                order_manager_->execute_buy(symbol2_, qty2, latest_prices_[symbol2_]);
            } else if (z_score < -entry_threshold_) {
                std::cout << "DEBUG: Executing LONG trade at price " << latest_prices_[symbol1_] << std::endl;
                int qty1 = 100;
                int qty2 = static_cast<int>(qty1 * hedge_ratio_);
                order_manager_->execute_buy(symbol1_, qty1, latest_prices_[symbol1_]);
                order_manager_->execute_sell(symbol2_, qty2, latest_prices_[symbol2_]);
            }
        } else if (std::abs(z_score) < exit_threshold_) {
            std::cout << "DEBUG: Executing EXIT trade" << std::endl;
            int position_s2 = order_manager_->get_position(symbol2_);
            if (position_s1 > 0) {
                order_manager_->execute_sell(symbol1_, std::abs(position_s1), latest_prices_[symbol1_]);
                order_manager_->execute_buy(symbol2_, std::abs(position_s2), latest_prices_[symbol2_]);
            } else {
                order_manager_->execute_buy(symbol1_, std::abs(position_s1), latest_prices_[symbol1_]);
                order_manager_->execute_sell(symbol2_, std::abs(position_s2), latest_prices_[symbol2_]);
            }
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