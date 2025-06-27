#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/strategy/MovingAverage.h"
#include "qse/strategy/MovingStandardDeviation.h"
#include "qse/order/IOrderManager.h"
#include "qse/data/Data.h"

#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>
#include <deque>

namespace qse {

    /**
     * @struct FactorData
     * @brief Represents factor exposure data for a single asset at a point in time
     */
    struct FactorData {
        std::string symbol;
        Timestamp timestamp;
        double momentum;      // 12M-1M returns
        double volatility;    // 20-day rolling volatility
        double value;         // Value factor (P/E proxy or return-based)
        double composite_score; // Z-score of combined factors
        
        FactorData() = default;
        FactorData(const std::string& sym, Timestamp ts, double mom, double vol, double val, double comp)
            : symbol(sym), timestamp(ts), momentum(mom), volatility(vol), value(val), composite_score(comp) {}
    };

    /**
     * @struct FactorReturns
     * @brief Daily factor return estimates from cross-sectional regression
     */
    struct FactorReturns {
        Timestamp timestamp;
        double momentum_return;
        double volatility_return;
        double value_return;
        double intercept;     // Market return component
        
        FactorReturns() = default;
        FactorReturns(Timestamp ts, double mom_ret, double vol_ret, double val_ret, double intercept)
            : timestamp(ts), momentum_return(mom_ret), volatility_return(vol_ret), 
              value_return(val_ret), intercept(intercept) {}
    };

    /**
     * @struct PortfolioWeights
     * @brief Target portfolio weights for each asset
     */
    struct PortfolioWeights {
        std::string symbol;
        double weight;        // Target weight (-1.0 to 1.0 for long/short)
        double sector_neutral_weight; // Sector-neutral adjusted weight
        
        PortfolioWeights() = default;
        PortfolioWeights(const std::string& sym, double w, double snw)
            : symbol(sym), weight(w), sector_neutral_weight(snw) {}
    };

    /**
     * @class MultiFactorStrategy
     * @brief Implements a multi-factor model strategy with factor computation and portfolio construction
     *
     * This strategy computes three main factors:
     * 1. Momentum: 12-month minus 1-month returns
     * 2. Volatility: 20-day rolling standard deviation of returns
     * 3. Value: Proxy based on recent return patterns (can be enhanced with fundamental data)
     *
     * The strategy then:
     * - Normalizes factors to z-scores
     * - Computes composite scores
     * - Performs cross-sectional regression to estimate factor returns
     * - Constructs long/short portfolios based on factor rankings
     * - Applies sector neutrality and other constraints
     */
    class MultiFactorStrategy : public IStrategy {
    public:
        /**
         * @brief Constructor for the MultiFactorStrategy
         * @param symbols List of symbols to trade
         * @param momentum_window Long-term window for momentum calculation (default: 252 days)
         * @param momentum_short_window Short-term window for momentum calculation (default: 21 days)
         * @param volatility_window Window for volatility calculation (default: 20 days)
         * @param value_window Window for value factor calculation (default: 60 days)
         * @param rebalance_frequency Days between portfolio rebalancing (default: 5)
         * @param top_n Number of assets to go long (default: 10)
         * @param bottom_n Number of assets to go short (default: 10)
         * @param order_manager Shared pointer to the order manager
         */
        MultiFactorStrategy(
            const std::vector<std::string>& symbols,
            int momentum_window = 252,
            int momentum_short_window = 21,
            int volatility_window = 20,
            int value_window = 60,
            int rebalance_frequency = 5,
            int top_n = 10,
            int bottom_n = 10,
            std::shared_ptr<IOrderManager> order_manager = nullptr
        );

        // Process an incoming tick (ignores all ticks)
        void on_tick(const Tick& tick) override;

        // Process an incoming bar - main logic for factor computation and portfolio construction
        void on_bar(const Bar& bar) override;

        // Process fills to track position changes
        void on_fill(const Fill& fill) override;

    private:
        // Factor computation methods
        double compute_momentum_factor(const std::string& symbol);
        double compute_volatility_factor(const std::string& symbol);
        double compute_value_factor(const std::string& symbol);
        
        // Normalization and scoring
        void normalize_factors();
        void compute_composite_scores();
        
        // Cross-sectional regression
        FactorReturns perform_cross_sectional_regression();
        
        // Portfolio construction
        std::vector<PortfolioWeights> construct_portfolio();
        void apply_sector_neutrality(std::vector<PortfolioWeights>& weights);
        void apply_turnover_penalty(std::vector<PortfolioWeights>& weights);
        
        // Utility methods
        double calculate_return(const std::string& symbol);
        void update_price_history(const std::string& symbol, double price);
        bool should_rebalance();
        void execute_portfolio_changes(const std::vector<PortfolioWeights>& target_weights);
        
        // Data storage
        std::vector<std::string> symbols_;
        std::map<std::string, std::deque<double>> price_history_;
        std::map<std::string, std::deque<double>> return_history_;
        std::map<std::string, Bar> latest_bars_;
        
        // Factor computation parameters
        int momentum_window_;
        int momentum_short_window_;
        int volatility_window_;
        int value_window_;
        
        // Portfolio parameters
        int rebalance_frequency_;
        int top_n_;
        int bottom_n_;
        int days_since_rebalance_;
        
        // Factor data storage
        std::map<std::string, FactorData> current_factors_;
        std::vector<FactorReturns> factor_return_history_;
        std::map<std::string, double> current_weights_;
        
        // Order management
        std::shared_ptr<IOrderManager> order_manager_;
        
        // Moving average utilities for factor computation (using pointers to avoid default constructor issues)
        std::map<std::string, std::unique_ptr<MovingAverage>> long_momentum_ma_;
        std::map<std::string, std::unique_ptr<MovingAverage>> short_momentum_ma_;
        std::map<std::string, std::unique_ptr<MovingStandardDeviation>> volatility_std_;
        std::map<std::string, std::unique_ptr<MovingAverage>> value_ma_;
    };

} // namespace qse 