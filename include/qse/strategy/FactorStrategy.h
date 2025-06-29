#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/exe/FactorExecutionEngine.h"
#include "qse/data/Data.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace qse {

/**
 * @brief Strategy that implements factor-based portfolio management using FactorExecutionEngine.
 * 
 * This strategy loads daily factor weights and rebalances the portfolio accordingly.
 * It integrates with the backtester to provide a complete factor execution pipeline.
 */
class FactorStrategy : public IStrategy {
public:
    /**
     * @brief Constructor
     * @param order_manager Shared pointer to the order manager
     * @param symbol The symbol this strategy trades
     * @param weights_dir Directory containing daily weight files
     * @param min_dollar_threshold Minimum dollar amount for rebalancing trades
     * @param engine_config Configuration for the factor execution engine
     */
    FactorStrategy(std::shared_ptr<IOrderManager> order_manager,
                   std::string symbol,
                   const std::string& weights_dir,
                   double min_dollar_threshold,
                   const ExecConfig& engine_config = ExecConfig{});
    
    virtual ~FactorStrategy() = default;

    // --- IStrategy interface implementation ---
    
    /**
     * @brief Handle incoming tick data
     * @param tick The tick data
     */
    void on_tick(const Tick& tick) override;
    
    /**
     * @brief Handle new bar data
     * @param bar The bar data
     */
    void on_bar(const Bar& bar) override;
    
    /**
     * @brief Handle order fills
     * @param fill The fill data
     */
    void on_fill(const Fill& fill) override;
    
    // --- FactorStrategy specific methods ---
    
    /**
     * @brief Handle end-of-day processing (rebalancing)
     * @param timestamp End of day timestamp
     */
    void on_day_close(const Timestamp& timestamp);

    /**
     * @brief Handle end-of-day processing with provided close prices
     * @param timestamp End of day timestamp
     * @param close_prices Map of symbol to close price
     */
    void on_day_close_with_prices(const Timestamp& timestamp, 
                                 const std::unordered_map<std::string, double>& close_prices);

private:
    struct Delta {
        std::string symbol;
        double target_weight;
        double current_weight;
        double delta_weight;
    };
    void compute_and_submit_delta_orders(const std::unordered_map<std::string, double>& close_prices);
    bool should_rebalance(const Timestamp& timestamp) const;
    
    std::shared_ptr<IOrderManager> order_manager_;
    std::string symbol_;
    std::string weights_dir_;
    double min_dollar_threshold_;
    std::shared_ptr<FactorExecutionEngine> engine_;
    
    // Current holdings cache
    std::unordered_map<std::string, double> current_holdings_;
    
    // Current target weights
    std::unordered_map<std::string, double> target_weights_;
    
    // Last rebalance date to prevent duplicate rebalancing
    mutable std::chrono::system_clock::time_point last_rebalance_;
};

} // namespace qse 