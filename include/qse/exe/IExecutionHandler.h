#pragma once

#include "qse/data/Data.h"

#include <functional>
#include <optional>
#include <string>

namespace qse {

/**
 * @brief Venue-agnostic execution interface (roadmap E1).
 *
 * Decouples order routing from the venue behind it: the backtest engine
 * (SimulatedExecutionHandler) and a live brokerage (AlpacaExecutionHandler,
 * E2) implement the same contract, so a strategy or runner written against
 * this interface moves from simulation to paper trading without changing a
 * line. The surface intentionally mirrors a REST brokerage: submit
 * market/limit, cancel, replace, and an asynchronous fill stream.
 */
class IExecutionHandler {
public:
    using FillCallback = std::function<void(const Fill&)>;

    virtual ~IExecutionHandler() = default;

    /// Submits a market order; returns the venue-assigned order id
    /// (empty on rejection).
    virtual OrderId submit_market_order(const std::string& symbol, Order::Side side,
                                        Volume quantity) = 0;

    /// Submits a limit order; returns the venue-assigned order id
    /// (empty on rejection).
    virtual OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity,
                                       Price limit_price,
                                       Order::TimeInForce tif = Order::TimeInForce::DAY) = 0;

    /// Cancels a working order. False if unknown or no longer cancellable.
    virtual bool cancel_order(const OrderId& order_id) = 0;

    /// Replaces a working limit order's quantity and limit price
    /// (cancel-and-resubmit semantics on venues without a native replace).
    /// Returns the working order id after the replace — which may differ
    /// from the original — or an empty id on failure.
    virtual OrderId replace_order(const OrderId& order_id, Volume new_quantity,
                                  Price new_limit_price) = 0;

    /// Queries the current state of an order at the venue.
    virtual std::optional<Order> get_order(const OrderId& order_id) const = 0;

    /// Registers the asynchronous fill stream.
    virtual void set_fill_callback(FillCallback callback) = 0;
};

} // namespace qse
