#include "qse/exe/AlpacaExecutionHandler.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>

using nlohmann::json;

namespace {

// Alpaca serializes numbers as JSON strings ("qty":"100"), and nullable
// fields (filled_avg_price before any fill) come back as null
double num_or(const json& j, const char* key, double fallback) {
    if (!j.contains(key) || j[key].is_null()) {
        return fallback;
    }
    if (j[key].is_string()) {
        try {
            return std::stod(j[key].get<std::string>());
        } catch (const std::exception&) {
            return fallback;
        }
    }
    if (j[key].is_number()) {
        return j[key].get<double>();
    }
    return fallback;
}

std::string side_to_string(qse::Order::Side side) {
    return side == qse::Order::Side::BUY ? "buy" : "sell";
}

std::string tif_to_string(qse::Order::TimeInForce tif) {
    switch (tif) {
    case qse::Order::TimeInForce::IOC:
        return "ioc";
    case qse::Order::TimeInForce::GTC:
        return "gtc";
    case qse::Order::TimeInForce::DAY:
        break;
    }
    return "day";
}

qse::Order::Status map_status(const std::string& alpaca_status) {
    if (alpaca_status == "filled") {
        return qse::Order::Status::FILLED;
    }
    if (alpaca_status == "partially_filled") {
        return qse::Order::Status::PARTIALLY_FILLED;
    }
    if (alpaca_status == "canceled" || alpaca_status == "pending_cancel" ||
        alpaca_status == "expired" || alpaca_status == "replaced" ||
        alpaca_status == "done_for_day") {
        return qse::Order::Status::CANCELLED;
    }
    if (alpaca_status == "rejected" || alpaca_status == "stopped" || alpaca_status == "suspended") {
        return qse::Order::Status::REJECTED;
    }
    return qse::Order::Status::PENDING; // new / accepted / pending_new / held
}

bool is_terminal(qse::Order::Status status) {
    return status == qse::Order::Status::FILLED || status == qse::Order::Status::CANCELLED ||
           status == qse::Order::Status::REJECTED;
}

qse::Order parse_order(const json& j) {
    qse::Order order;
    order.order_id = j.value("id", "");
    order.symbol = j.value("symbol", "");
    order.side = j.value("side", "buy") == std::string("buy") ? qse::Order::Side::BUY
                                                              : qse::Order::Side::SELL;
    order.type = j.value("type", "market") == std::string("limit") ? qse::Order::Type::LIMIT
                                                                   : qse::Order::Type::MARKET;
    order.quantity = static_cast<qse::Volume>(num_or(j, "qty", 0));
    order.filled_quantity = static_cast<qse::Volume>(num_or(j, "filled_qty", 0));
    order.avg_fill_price = num_or(j, "filled_avg_price", 0.0);
    order.limit_price = num_or(j, "limit_price", 0.0);
    order.status = map_status(j.value("status", "new"));
    std::string tif = j.value("time_in_force", "day");
    order.time_in_force = tif == "ioc"   ? qse::Order::TimeInForce::IOC
                          : tif == "gtc" ? qse::Order::TimeInForce::GTC
                                         : qse::Order::TimeInForce::DAY;
    order.timestamp = std::chrono::system_clock::now();
    return order;
}

} // namespace

namespace qse {

AlpacaExecutionHandler::AlpacaExecutionHandler(std::shared_ptr<IHttpClient> http,
                                               std::string key_id, std::string secret_key,
                                               std::string base_url)
    : http_(std::move(http)), key_id_(std::move(key_id)), secret_key_(std::move(secret_key)),
      base_url_(std::move(base_url)) {
    if (key_id_.empty() || secret_key_.empty()) {
        throw std::runtime_error("AlpacaExecutionHandler requires non-empty API credentials");
    }
}

AlpacaExecutionHandler AlpacaExecutionHandler::from_env(std::shared_ptr<IHttpClient> http,
                                                        std::string base_url) {
    const char* key_id = std::getenv("APCA_API_KEY_ID");
    const char* secret = std::getenv("APCA_API_SECRET_KEY");
    if (key_id == nullptr || secret == nullptr) {
        throw std::runtime_error("Set APCA_API_KEY_ID and APCA_API_SECRET_KEY in the environment "
                                 "(see .env.example); credentials are never read from the repo");
    }
    return AlpacaExecutionHandler(std::move(http), key_id, secret, std::move(base_url));
}

std::vector<std::string> AlpacaExecutionHandler::auth_headers() const {
    return {"APCA-API-KEY-ID: " + key_id_, "APCA-API-SECRET-KEY: " + secret_key_,
            "Content-Type: application/json"};
}

OrderId AlpacaExecutionHandler::submit(const std::string& body_json) {
    HttpResponse response = http_->post(base_url_ + "/v2/orders", auth_headers(), body_json);
    if (!response.ok()) {
        std::cerr << "[Alpaca] order rejected (HTTP " << response.status << "): " << response.body
                  << std::endl;
        return {};
    }
    try {
        auto parsed = json::parse(response.body);
        OrderId id = parsed.value("id", "");
        if (!id.empty()) {
            emitted_fill_qty_[id] = 0; // start tracking for poll_fills
        }
        return id;
    } catch (const std::exception& e) {
        std::cerr << "[Alpaca] unparseable order response: " << e.what() << std::endl;
        return {};
    }
}

OrderId AlpacaExecutionHandler::submit_market_order(const std::string& symbol, Order::Side side,
                                                    Volume quantity) {
    json body = {{"symbol", symbol},
                 {"qty", std::to_string(quantity)},
                 {"side", side_to_string(side)},
                 {"type", "market"},
                 {"time_in_force", "day"}};
    return submit(body.dump());
}

OrderId AlpacaExecutionHandler::submit_limit_order(const std::string& symbol, Order::Side side,
                                                   Volume quantity, Price limit_price,
                                                   Order::TimeInForce tif) {
    json body = {{"symbol", symbol},
                 {"qty", std::to_string(quantity)},
                 {"side", side_to_string(side)},
                 {"type", "limit"},
                 {"limit_price", std::to_string(limit_price)},
                 {"time_in_force", tif_to_string(tif)}};
    return submit(body.dump());
}

bool AlpacaExecutionHandler::cancel_order(const OrderId& order_id) {
    HttpResponse response = http_->del(base_url_ + "/v2/orders/" + order_id, auth_headers());
    return response.ok(); // Alpaca answers 204 on success
}

OrderId AlpacaExecutionHandler::replace_order(const OrderId& order_id, Volume new_quantity,
                                              Price new_limit_price) {
    json body = {{"qty", std::to_string(new_quantity)},
                 {"limit_price", std::to_string(new_limit_price)}};
    HttpResponse response =
        http_->patch(base_url_ + "/v2/orders/" + order_id, auth_headers(), body.dump());
    if (!response.ok()) {
        return {};
    }
    try {
        auto parsed = json::parse(response.body);
        OrderId new_id = parsed.value("id", "");
        if (!new_id.empty() && new_id != order_id) {
            // Alpaca issues a fresh order id on replace: carry over the
            // already-emitted fill count so partial fills are not re-emitted
            emitted_fill_qty_[new_id] = emitted_fill_qty_[order_id];
            emitted_fill_qty_.erase(order_id);
        }
        return new_id;
    } catch (const std::exception&) {
        return {};
    }
}

std::optional<Order> AlpacaExecutionHandler::get_order(const OrderId& order_id) const {
    HttpResponse response = http_->get(base_url_ + "/v2/orders/" + order_id, auth_headers());
    if (!response.ok()) {
        return std::nullopt;
    }
    try {
        return parse_order(json::parse(response.body));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void AlpacaExecutionHandler::set_fill_callback(FillCallback callback) {
    fill_callback_ = std::move(callback);
}

std::size_t AlpacaExecutionHandler::poll_fills() {
    std::size_t emitted = 0;
    for (auto it = emitted_fill_qty_.begin(); it != emitted_fill_qty_.end();) {
        auto order = get_order(it->first);
        if (!order) {
            ++it;
            continue; // transient failure: retry on the next poll
        }
        if (order->filled_quantity > it->second) {
            Volume delta = order->filled_quantity - it->second;
            if (fill_callback_) {
                Fill fill(order->order_id, order->symbol, delta, order->avg_fill_price,
                          order->timestamp, order->side == Order::Side::BUY ? "BUY" : "SELL");
                fill_callback_(fill);
            }
            it->second = order->filled_quantity;
            ++emitted;
        }
        if (is_terminal(order->status)) {
            it = emitted_fill_qty_.erase(it); // nothing more can happen
        } else {
            ++it;
        }
    }
    return emitted;
}

} // namespace qse
