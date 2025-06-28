#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include "qse/order/IOrderManager.h"

namespace qse {

/**
 * @brief Configuration parameters for the FactorExecutionEngine.
 */
struct ExecConfig {
    // Time of day (HH:MM, 24h) when rebalancing should occur
    std::string rebal_time{"15:45"};

    // Order style: "target_percent" or "market"
    std::string order_style{"target_percent"};

    // Maximum allowed price impact per trade (as fraction of ADV, etc.)
    double max_px_impact{0.01};

    // Minimum notional per order to avoid dust orders (USD)
    double min_notional{100.0};

    // Rounding lot size (shares)
    int lot_size{1};
};

/**
 * @brief Engine responsible for converting daily factor weight files into executable orders.
 *
 * Micro-steps H-1 … H-6 will progressively flesh out this class.  For now we only define the
 * interface to keep compilation green.
 */
class FactorExecutionEngine {
public:
    /**
     * @param cfg            Execution configuration parameters.
     * @param order_manager  Shared pointer to an order manager implementation used to route orders.
     */
    FactorExecutionEngine(ExecConfig cfg, std::shared_ptr<IOrderManager> order_manager)
        : cfg_(std::move(cfg)), order_manager_(std::move(order_manager)) {}

    // --------------------------- Future API placeholders ---------------------------

    // (H-1) Load weight file (CSV or Parquet) – to be implemented
    std::unordered_map<std::string, double> load_weights(const std::string& path);

    // (H-2) Capture current holdings snapshot – to be implemented
    std::unordered_map<std::string, double> fetch_holdings();

    // (H-3) Calculate target share differences – to be implemented
    std::unordered_map<std::string, long long> calc_target_shares(
        const std::unordered_map<std::string, double>& target_weights,
        const std::unordered_map<std::string, double>& current_holdings,
        double cash,
        const std::unordered_map<std::string, double>& prices);

    // (H-5) Submit orders to OrderManager – to be implemented
    void submit_orders(const std::unordered_map<std::string, long long>& target_qty);

    // (H-6) Rebalance gatekeeping – to be implemented
    bool should_rebalance(std::chrono::system_clock::time_point now) const;

private:
    ExecConfig cfg_{};
    std::shared_ptr<IOrderManager> order_manager_;

    // Track last rebalance date to prevent duplicate runs (H-6)
    std::chrono::system_clock::time_point last_rebalance_{};
};

} // namespace qse 