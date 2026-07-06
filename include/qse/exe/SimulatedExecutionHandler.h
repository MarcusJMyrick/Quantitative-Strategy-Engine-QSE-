#pragma once

#include "qse/exe/IExecutionHandler.h"
#include "qse/order/IOrderManager.h"

#include <utility>

namespace qse {

/**
 * @brief IExecutionHandler backed by the backtest engine (roadmap E1).
 *
 * Adapts the existing OrderManager fill simulation — full-depth book walks,
 * queue-position limit fills, or the naive model, whichever the injected
 * OrderManager is configured for — behind the venue-agnostic interface.
 * Everything a strategy submits here goes through exactly the same
 * matching/fill/accounting logic the backtests exercise.
 *
 * The handler takes over the OrderManager's fill callback: consumers must
 * subscribe to fills through set_fill_callback on the handler, not on the
 * underlying OrderManager.
 */
class SimulatedExecutionHandler : public IExecutionHandler {
public:
    explicit SimulatedExecutionHandler(IOrderManager& order_manager)
        : order_manager_(order_manager) {
        order_manager_.set_fill_callback([this](const Fill& fill) {
            if (fill_callback_) {
                fill_callback_(fill);
            }
        });
    }

    OrderId submit_market_order(const std::string& symbol, Order::Side side,
                                Volume quantity) override {
        return order_manager_.submit_market_order(symbol, side, quantity);
    }

    OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity,
                               Price limit_price, Order::TimeInForce tif) override {
        return order_manager_.submit_limit_order(symbol, side, quantity, limit_price, tif);
    }

    bool cancel_order(const OrderId& order_id) override {
        return order_manager_.cancel_order(order_id);
    }

    OrderId replace_order(const OrderId& order_id, Volume new_quantity,
                          Price new_limit_price) override {
        // Cancel-and-resubmit: the simulated venue has no native replace.
        // Only working limit orders are replaceable, matching brokerage
        // semantics (a market order is gone the moment it is accepted).
        auto existing = order_manager_.get_order(order_id);
        if (!existing || !existing->is_active() || existing->type != Order::Type::LIMIT) {
            return {};
        }
        if (!order_manager_.cancel_order(order_id)) {
            return {};
        }
        return order_manager_.submit_limit_order(existing->symbol, existing->side, new_quantity,
                                                 new_limit_price, existing->time_in_force);
    }

    std::optional<Order> get_order(const OrderId& order_id) const override {
        return order_manager_.get_order(order_id);
    }

    void set_fill_callback(FillCallback callback) override { fill_callback_ = std::move(callback); }

private:
    IOrderManager& order_manager_;
    FillCallback fill_callback_;
};

} // namespace qse
