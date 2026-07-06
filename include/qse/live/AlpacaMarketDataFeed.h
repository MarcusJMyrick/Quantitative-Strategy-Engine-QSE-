#pragma once

#include "qse/core/SPSCRingBuffer.h"
#include "qse/data/Data.h"
#include "qse/exe/IHttpClient.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace qse {

/**
 * @brief Live quote feed from the Alpaca market-data REST API (E3).
 *
 * A network thread polls the latest-quote endpoint (free IEX feed) and pushes
 * de-duplicated ticks into the G2 SPSC ring buffer; the strategy thread
 * drains at its own pace. Same producer/consumer architecture a websocket
 * feed would use - the transport can be upgraded without touching consumers.
 * The HTTP layer is injected, so tests run against a fake with zero network.
 */
class AlpacaMarketDataFeed {
public:
    AlpacaMarketDataFeed(std::shared_ptr<IHttpClient> http, std::string key_id,
                         std::string secret_key, std::string symbol,
                         std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000),
                         std::string base_url = "https://data.alpaca.markets",
                         std::size_t ring_capacity = 4096);

    ~AlpacaMarketDataFeed();

    AlpacaMarketDataFeed(const AlpacaMarketDataFeed&) = delete;
    AlpacaMarketDataFeed& operator=(const AlpacaMarketDataFeed&) = delete;

    /// Starts the polling (producer) thread.
    void start();

    /// Stops and joins the polling thread.
    void stop();

    /// Consumer thread only: drains queued ticks into the handler in one
    /// batch. Returns the number of ticks processed.
    std::size_t drain(const std::function<void(const Tick&)>& handler);

    /// Polls once on the caller's thread (used by tests and the start loop).
    /// Returns true if a new (non-duplicate) quote was pushed.
    bool poll_once();

    std::size_t dropped_ticks() const { return dropped_.load(std::memory_order_relaxed); }

private:
    std::shared_ptr<IHttpClient> http_;
    std::string key_id_;
    std::string secret_key_;
    std::string symbol_;
    std::chrono::milliseconds poll_interval_;
    std::string base_url_;

    SPSCRingBuffer<Tick> ring_;
    std::thread poll_thread_;
    std::atomic<bool> running_{false};
    std::atomic<std::size_t> dropped_{0};
    std::string last_quote_stamp_; // producer-thread only: de-dup key
};

} // namespace qse
