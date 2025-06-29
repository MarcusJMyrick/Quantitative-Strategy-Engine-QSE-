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
     * @param engine Shared pointer to the factor execution engine
     */
    explicit FactorStrategy(std::shared_ptr<FactorExecutionEngine> engine);
    
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

private:
    std::shared_ptr<FactorExecutionEngine> engine_;
    
    // Current holdings cache
    std::unordered_map<std::string, double> current_holdings_;
    
    // Current target weights
    std::unordered_map<std::string, double> target_weights_;
    
    // Last rebalance date to prevent duplicate rebalancing
    mutable std::chrono::system_clock::time_point last_rebalance_;
};

} // namespace qse 