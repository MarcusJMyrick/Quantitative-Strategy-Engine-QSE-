#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/data/Data.h"
#include "qse/order/IOrderManager.h"
#include <vector>
#include <memory>

namespace qse {

/**
 * @brief A strategy that tracks its own fills to test the fill callback system
 * 
 * This strategy submits orders and tracks all fills it receives via the on_fill callback.
 * It's useful for testing the strategy → order manager → order book → fills pipeline.
 */
class FillTrackingStrategy : public IStrategy {
public:
    FillTrackingStrategy(std::shared_ptr<IOrderManager> order_manager);
    
    // Override on_tick to submit test orders
    void on_tick(const Tick& tick) override;
    
    // Override on_fill to track fills
    void on_fill(const Fill& fill) override;
    
    // Get the fills this strategy has received
    const std::vector<Fill>& get_fills() const { return fills_; }
    
    // Get the number of fills received
    size_t get_fill_count() const { return fills_.size(); }
    
    // Clear fills (for testing)
    void clear_fills() { fills_.clear(); }

private:
    std::shared_ptr<IOrderManager> order_manager_;
    std::vector<Fill> fills_;
    bool order_submitted_ = false;
};

} // namespace qse 