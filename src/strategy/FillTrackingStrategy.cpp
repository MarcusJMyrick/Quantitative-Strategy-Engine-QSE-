#include "qse/strategy/FillTrackingStrategy.h"
#include <iostream>

namespace qse {

FillTrackingStrategy::FillTrackingStrategy(std::shared_ptr<IOrderManager> order_manager)
    : order_manager_(order_manager) {
}

void FillTrackingStrategy::on_tick(const Tick& tick) {
    // Submit a test market order on the first tick
    if (!order_submitted_) {
        order_manager_->submit_market_order(tick.symbol, Order::Side::BUY, 100);
        order_submitted_ = true;
    }
}

void FillTrackingStrategy::on_fill(const Fill& fill) {
    fills_.push_back(fill);
}

} // namespace qse 