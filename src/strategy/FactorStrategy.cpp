#include "qse/strategy/FactorStrategy.h"
#include "qse/strategy/WeightsLoader.h"
#include <iostream>

namespace qse {

FactorStrategy::FactorStrategy(std::shared_ptr<FactorExecutionEngine> engine)
    : engine_(std::move(engine))
    , last_rebalance_(std::chrono::system_clock::time_point::min()) {
    if (!engine_) {
        throw std::invalid_argument("FactorStrategy requires a valid FactorExecutionEngine");
    }
}

void FactorStrategy::on_tick(const Tick& tick) {
    // TODO: I-3 - Implement delta-to-orders logic
    // For now, just pass through to engine for potential slippage modeling
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
    std::string weights_dir = "weights";  // TODO: Make configurable
    auto new_weights = WeightsLoader::load_daily_weights(weights_dir, timestamp);
    
    if (new_weights) {
        target_weights_ = *new_weights;
        std::cout << "[FACTOR] Loaded " << target_weights_.size() << " weights for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    } else {
        std::cout << "[FACTOR] No weights file found for date: " 
                  << WeightsLoader::date_to_string(timestamp) << std::endl;
    }
    
    // TODO: I-3 - Calculate and submit orders
    // TODO: I-5 - Implement rebalance guard
}

} // namespace qse 