#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/strategy/MovingAverage.h"
#include "qse/strategy/MovingStandardDeviation.h"
#include "qse/order/IOrderManager.h"
#include "qse/data/Data.h"

#include <string>
#include <memory>
#include <map>

namespace qse {

    /**
     * @class PairsTradingStrategy
     * @brief Implements a statistical arbitrage pairs trading strategy.
     *
     * This strategy monitors the spread between two cointegrated assets. It calculates
     * the z-score of the spread relative to its moving average and standard deviation.
     * It enters a position when the spread deviates significantly from its mean (high or low z-score)
     * and exits when the spread reverts back towards the mean.
     */
    class PairsTradingStrategy : public IStrategy {
    public:
        /**
         * @brief Constructor for the PairsTradingStrategy.
         * @param symbol1 The first asset in the pair (e.g., "SPY").
         * @param symbol2 The second asset in the pair (e.g., "IVV").
         * @param hedge_ratio The ratio for calculating the spread (price1 - ratio * price2).
         * @param spread_window The lookback window for calculating the spread's mean and std dev.
         * @param entry_threshold The z-score value at which to enter a trade (e.g., 2.0).
         * @param exit_threshold The z-score value at which to exit a trade (e.g., 0.5).
         * @param order_manager A shared pointer to the order manager for executing trades.
         */
        PairsTradingStrategy(
            const std::string& symbol1,
            const std::string& symbol2,
            double hedge_ratio,
            int spread_window,
            double entry_threshold,
            double exit_threshold,
            std::shared_ptr<IOrderManager> order_manager
        );

        // Process an incoming tick. This is the core logic of the strategy.
        void on_tick(const Tick& tick) override;

        // Process an incoming bar. (Not used in this tick-driven strategy).
        void on_bar(const Bar& bar) override;

        // Update the price for a specific symbol and check for trading opportunities.
        void update_price(const std::string& symbol, double price);

    private:
        // Checks the trading conditions and executes orders if necessary.
        void check_and_execute_trades();

        // The two symbols in the pair.
        std::string symbol1_;
        std::string symbol2_;

        // The hedge ratio calculated from OLS regression.
        double hedge_ratio_;
        
        // Z-score thresholds for entering and exiting trades.
        double entry_threshold_;
        double exit_threshold_;

        // Pointer to the order manager to execute trades.
        std::shared_ptr<IOrderManager> order_manager_;

        // Utilities to calculate the moving average and standard deviation of the spread.
        MovingAverage spread_mean_;
        MovingStandardDeviation spread_std_dev_;

        // Internal state to store the most recent price of each asset.
        std::map<std::string, double> latest_prices_;
    };

} // namespace qse 