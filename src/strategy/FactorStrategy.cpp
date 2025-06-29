#include "qse/strategy/FactorStrategy.h"
#include "qse/strategy/WeightsLoader.h"
#include "qse/order/IOrderManager.h"
#include <iostream>

namespace qse {

FactorStrategy::FactorStrategy(std::shared_ptr<IOrderManager> order_manager,
                               std::string symbol,
                               const std::string& weights_dir,
                               double min_dollar_threshold,
                               const ExecConfig& engine_config)
    : order_manager_(std::move(order_manager))
    , symbol_(std::move(symbol))
    , weights_dir_(weights_dir)
    , min_dollar_threshold_(min_dollar_threshold)
    , engine_(std::make_shared<FactorExecutionEngine>(engine_config, order_manager_))
    , last_rebalance_(std::chrono::system_clock::time_point::min()) {
    if (!order_manager_) {
        throw std::invalid_argument("FactorStrategy requires a valid OrderManager");
    }
}

void FactorStrategy::on_tick(const Tick& tick) {
    // I-6: Pass ticks to FactorExecutionEngine for slippage modeling
    // The engine can use tick data for market impact and slippage calculations
    if (engine_) {
        // TODO: Add tick processing method to FactorExecutionEngine if needed
        // For now, the engine can access tick data through the order manager
    }
}

void FactorStrategy::on_bar(const Bar& bar) {
    // TODO: I-3 - Implement bar-based rebalancing logic
    // Check if this bar represents end-of-day and trigger rebalancing
}

void FactorStrategy::on_fill(const Fill& fill) {
    // TODO: I-4 - Implement fill tracking
    // Update internal holdings cache when orders are filled
    if (fill.side == "BUY") {
        current_holdings_[fill.symbol] += fill.quantity;
    } else if (fill.side == "SELL") {
        current_holdings_[fill.symbol] -= fill.quantity;
    }
}

void FactorStrategy::on_day_close(const Timestamp& timestamp) {
    // I-2: Load daily weights
    auto new_weights = WeightsLoader::load_daily_weights(weights_dir_, timestamp);
    
    if (new_weights) {
        target_weights_ = *new_weights;
        std::cout << "[FACTOR] Loaded " << target_weights_.size() << " weights for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    } else {
        std::cout << "[FACTOR] No weights file found for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    }
    
    // TODO: I-3 - Calculate and submit orders
    // For now, simulate close prices for all symbols (in real code, pass in actual close prices)
    std::unordered_map<std::string, double> close_prices;
    for (const auto& [sym, _] : target_weights_) {
        close_prices[sym] = 100.0; // TODO: Replace with real close prices
    }
    on_day_close_with_prices(timestamp, close_prices);
    // TODO: I-5 - Implement rebalance guard
}

void FactorStrategy::on_day_close_with_prices(const Timestamp& timestamp, 
                                             const std::unordered_map<std::string, double>& close_prices) {
    // I-5: Rebalance guard - check if we should rebalance
    if (!should_rebalance(timestamp)) {
        std::cout << "[FACTOR] Skipping rebalance - already rebalanced today" << std::endl;
        return;
    }
    
    // I-2: Load daily weights
    auto new_weights = WeightsLoader::load_daily_weights(weights_dir_, timestamp);
    
    if (new_weights) {
        target_weights_ = *new_weights;
        std::cout << "[FACTOR] Loaded " << target_weights_.size() << " weights for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    } else {
        std::cout << "[FACTOR] No weights file found for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    }
    
    compute_and_submit_delta_orders(close_prices);
    
    // Update last rebalance timestamp
    last_rebalance_ = timestamp;
}

bool FactorStrategy::should_rebalance(const Timestamp& timestamp) const {
    // If no previous rebalance, allow it
    if (last_rebalance_ == std::chrono::system_clock::time_point::min()) {
        return true;
    }
    
    // Convert timestamps to date strings for comparison
    std::string current_date = WeightsLoader::date_to_string(timestamp);
    std::string last_date = WeightsLoader::date_to_string(last_rebalance_);
    
    // Allow rebalance only if it's a different date
    return current_date != last_date;
}

void FactorStrategy::compute_and_submit_delta_orders(const std::unordered_map<std::string, double>& close_prices) {
    std::vector<Delta> deltas;
    
    // Cache positions to avoid double calls
    std::unordered_map<std::string, double> positions;
    for (const auto& [sym, px] : close_prices) {
        positions[sym] = order_manager_->get_position(sym);
    }
    
    double nav = order_manager_->get_cash(); // Start with cash
    // Add value of all positions
    for (const auto& [sym, px] : close_prices) {
        nav += positions[sym] * px;
    }
    
    if (nav == 0.0) {
        std::cerr << "[ERROR] NAV is zero, cannot rebalance." << std::endl;
        return;
    }
    
    for (const auto& [sym, w_target] : target_weights_) {
        double px = close_prices.at(sym);
        double pos = positions[sym];
        double w_cur = (pos * px) / nav;
        double d_w = w_target - w_cur;
        if (std::fabs(d_w * nav) < min_dollar_threshold_) continue;
        deltas.push_back({sym, w_target, w_cur, d_w});
    }
    
    for (const auto& d : deltas) {
        double px = close_prices.at(d.symbol);
        double dollars = d.delta_weight * nav;
        int qty = static_cast<int>(std::floor(std::fabs(dollars / px)));
        if (qty == 0) continue;
        if (d.delta_weight > 0)
            order_manager_->execute_buy(d.symbol, qty, px);
        else
            order_manager_->execute_sell(d.symbol, qty, px);
    }
}

} // namespace qse 