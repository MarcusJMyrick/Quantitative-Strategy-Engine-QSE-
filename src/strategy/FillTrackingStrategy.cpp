#include "qse/strategy/FillTrackingStrategy.h"
#include <iostream>

namespace qse {

FillTrackingStrategy::FillTrackingStrategy(std::shared_ptr<IOrderManager> order_manager)
    : order_manager_(order_manager) {
}

void FillTrackingStrategy::on_tick(const Tick& tick) {
    // Submit a test market order on the first tick
    if (!order_submitted_) {
        std::cout << "FillTrackingStrategy: Submitting test market buy order for 100 shares at tick " 
                  << tick.symbol << " @ " << tick.price << std::endl;
        
        OrderId order_id = order_manager_->submit_market_order(tick.symbol, Order::Side::BUY, 100);
        std::cout << "FillTrackingStrategy: Submitted order with ID: " << order_id << std::endl;
        
        order_submitted_ = true;
    }
}

void FillTrackingStrategy::on_fill(const Fill& fill) {
    std::cout << "FillTrackingStrategy: Received fill - Order " << fill.order_id 
              << " for " << fill.quantity << " shares of " << fill.symbol 
              << " at $" << fill.price << " (" << fill.side << ")" << std::endl;
    
    fills_.push_back(fill);
}

} // namespace qse 