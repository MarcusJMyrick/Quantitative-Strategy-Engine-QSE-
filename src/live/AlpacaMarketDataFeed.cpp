#include "qse/live/AlpacaMarketDataFeed.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <utility>

using nlohmann::json;

namespace qse {

AlpacaMarketDataFeed::AlpacaMarketDataFeed(std::shared_ptr<IHttpClient> http, std::string key_id,
                                           std::string secret_key, std::string symbol,
                                           std::chrono::milliseconds poll_interval,
                                           std::string base_url, std::size_t ring_capacity)
    : http_(std::move(http)), key_id_(std::move(key_id)), secret_key_(std::move(secret_key)),
      symbol_(std::move(symbol)), poll_interval_(poll_interval), base_url_(std::move(base_url)),
      ring_(ring_capacity) {}

AlpacaMarketDataFeed::~AlpacaMarketDataFeed() {
    stop();
}

void AlpacaMarketDataFeed::start() {
    if (running_.exchange(true)) {
        return;
    }
    poll_thread_ = std::thread([this] {
        while (running_.load(std::memory_order_relaxed)) {
            poll_once();
            std::this_thread::sleep_for(poll_interval_);
        }
    });
}

void AlpacaMarketDataFeed::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
}

std::size_t AlpacaMarketDataFeed::drain(const std::function<void(const Tick&)>& handler) {
    return ring_.consume_all([&handler](Tick&& tick) { handler(tick); });
}

bool AlpacaMarketDataFeed::poll_once() {
    const std::string url = base_url_ + "/v2/stocks/" + symbol_ + "/quotes/latest?feed=iex";
    HttpResponse response =
        http_->get(url, {"APCA-API-KEY-ID: " + key_id_, "APCA-API-SECRET-KEY: " + secret_key_});
    if (!response.ok()) {
        return false; // transient failure: next poll retries
    }

    try {
        auto parsed = json::parse(response.body);
        if (!parsed.contains("quote")) {
            return false;
        }
        const auto& quote = parsed["quote"];
        std::string stamp = quote.value("t", "");
        if (stamp.empty() || stamp == last_quote_stamp_) {
            return false; // no fresh quote since the last poll
        }
        last_quote_stamp_ = stamp;

        Tick tick;
        tick.symbol = symbol_;
        tick.timestamp = std::chrono::system_clock::now(); // arrival time
        tick.bid = quote.value("bp", 0.0);
        tick.ask = quote.value("ap", 0.0);
        tick.bid_size = static_cast<Volume>(quote.value("bs", 0));
        tick.ask_size = static_cast<Volume>(quote.value("as", 0));
        if (tick.bid <= 0.0 && tick.ask <= 0.0) {
            return false;
        }
        tick.price = tick.bid > 0.0 && tick.ask > 0.0 ? (tick.bid + tick.ask) / 2.0
                     : tick.ask > 0.0                 ? tick.ask
                                                      : tick.bid;
        tick.volume = 0; // quote update, not a trade print

        if (!ring_.try_push(tick)) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[AlpacaFeed] unparseable quote: " << e.what() << std::endl;
        return false;
    }
}

} // namespace qse
