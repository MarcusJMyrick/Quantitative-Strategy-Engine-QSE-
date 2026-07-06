#pragma once

#include "qse/exe/IExecutionHandler.h"
#include "qse/exe/IHttpClient.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace qse {

/**
 * @brief IExecutionHandler speaking REST to the Alpaca paper-trading API (E2).
 *
 * Implements the same contract as SimulatedExecutionHandler, so a strategy
 * moves from backtest to paper trading by swapping the injected handler.
 * The HTTP layer is injected (IHttpClient), so unit tests run against a mock
 * with zero network; production uses CurlHttpClient against
 * https://paper-api.alpaca.markets.
 *
 * Credentials come from the environment (APCA_API_KEY_ID /
 * APCA_API_SECRET_KEY, the official variable names) via from_env(), or are
 * passed explicitly. They are never read from files in the repository.
 *
 * Fills are surfaced by polling: poll_fills() queries each tracked order and
 * emits a Fill for any newly executed quantity since the last poll. The live
 * runner (E3) calls it on its cadence; a trade_updates websocket can replace
 * it later without touching the interface.
 */
class AlpacaExecutionHandler : public IExecutionHandler {
public:
    AlpacaExecutionHandler(std::shared_ptr<IHttpClient> http, std::string key_id,
                           std::string secret_key,
                           std::string base_url = "https://paper-api.alpaca.markets");

    /// Builds a handler from APCA_API_KEY_ID / APCA_API_SECRET_KEY; throws
    /// std::runtime_error if either is missing.
    static AlpacaExecutionHandler
    from_env(std::shared_ptr<IHttpClient> http,
             std::string base_url = "https://paper-api.alpaca.markets");

    // --- IExecutionHandler ---
    OrderId submit_market_order(const std::string& symbol, Order::Side side,
                                Volume quantity) override;
    OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity,
                               Price limit_price, Order::TimeInForce tif) override;
    bool cancel_order(const OrderId& order_id) override;
    OrderId replace_order(const OrderId& order_id, Volume new_quantity,
                          Price new_limit_price) override;
    std::optional<Order> get_order(const OrderId& order_id) const override;
    void set_fill_callback(FillCallback callback) override;

    /// Polls every tracked (non-terminal) order and emits a Fill for newly
    /// executed quantity. Returns the number of fills emitted.
    std::size_t poll_fills() override;

private:
    std::vector<std::string> auth_headers() const;
    OrderId submit(const std::string& body_json);

    std::shared_ptr<IHttpClient> http_;
    std::string key_id_;
    std::string secret_key_;
    std::string base_url_;
    FillCallback fill_callback_;

    // filled quantity already emitted per tracked order id
    std::unordered_map<OrderId, Volume> emitted_fill_qty_;
};

} // namespace qse
