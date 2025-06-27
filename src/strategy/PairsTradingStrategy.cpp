#include "qse/strategy/PairsTradingStrategy.h"
#define DEBUG

#include <iostream>
#include <cmath>
#include <chrono>

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

    // Ignore all ticks - this strategy is bar-driven
    void PairsTradingStrategy::on_tick(const Tick& tick) {
        // Do nothing - we only process bars
    }

    // Main strategy logic - process bars for our symbols
    void PairsTradingStrategy::on_bar(const Bar& bar) {
        // Only track bars for the symbols we care about
        if (bar.symbol != symbol1_ && bar.symbol != symbol2_) {
            return; // Ignore other symbols
        }

        // Store the latest bar for this symbol
        latest_bars_[bar.symbol] = bar;

        // We need bars for both symbols AND they must belong to the same time bucket
        if (latest_bars_.count(symbol1_) == 0 || latest_bars_.count(symbol2_) == 0) {
            return; // Still waiting for both legs
        }

        const Bar& bar1 = latest_bars_[symbol1_];
        const Bar& bar2 = latest_bars_[symbol2_];

        // Ensure the bars are from the same timestamp bucket
        if (bar1.timestamp != bar2.timestamp) {
            return; // Different buckets – wait until both align
        }

        std::cerr << "[Pairs] aligned bars ts=" << std::chrono::duration_cast<std::chrono::seconds>(bar1.timestamp.time_since_epoch()).count() << " close1=" << bar1.close << " close2=" << bar2.close << std::endl;

        // Update latest prices used by the existing trading logic
        latest_prices_[symbol1_] = bar1.close;
        latest_prices_[symbol2_] = bar2.close;

        // Now evaluate trading logic
        if (latest_prices_[symbol1_] > 0 && latest_prices_[symbol2_] > 0) {
            check_and_execute_trades();
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

        // 2. If indicators are not ready, just update them and wait for the next bar.
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

} // namespace qse 