#pragma once

#include "qse/core/SPSCRingBuffer.h"
#include "qse/data/Data.h"
#include "qse/messaging/TickSubscriber.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>

namespace qse {

/**
 * @brief Lock-free hand-off between a market-data network thread and the
 * strategy thread (G2, the wiring live mode builds on).
 *
 * The network thread runs the ZeroMQ subscriber and pushes every received
 * tick into an SPSC ring buffer; the strategy thread drains the ring at its
 * own pace. Because the ring is lock-free, a slow strategy can never block
 * the network thread - if the strategy falls behind by more than the ring
 * capacity, ticks are dropped and counted (visible backpressure beats an
 * invisible frozen feed).
 */
class LiveTickPipeline {
public:
    using TickHandler = std::function<void(const Tick&)>;

    explicit LiveTickPipeline(const std::string& endpoint, std::size_t ring_capacity = 16384)
        : ring_(ring_capacity), subscriber_(endpoint, "TICK_DATA") {
        subscriber_.set_tick_callback([this](const Tick& tick) {
            if (!ring_.try_push(tick)) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    ~LiveTickPipeline() { stop(); }

    LiveTickPipeline(const LiveTickPipeline&) = delete;
    LiveTickPipeline& operator=(const LiveTickPipeline&) = delete;

    /// Starts the network (producer) thread.
    void start() {
        if (running_.exchange(true)) {
            return; // already running
        }
        network_thread_ = std::thread([this] {
            while (running_.load(std::memory_order_relaxed)) {
                if (!subscriber_.try_receive()) {
                    // Nothing pending; yield instead of burning the core
                    std::this_thread::yield();
                }
            }
        });
    }

    /// Stops the network thread and joins it.
    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        if (network_thread_.joinable()) {
            network_thread_.join();
        }
    }

    /// Consumer thread only: pops one tick if available.
    bool try_pop(Tick& out) { return ring_.try_pop(out); }

    /// Consumer thread only: drains everything currently queued into the
    /// handler in one batch (single acquire/release pair on the ring).
    /// Returns the number of ticks processed.
    std::size_t drain(const TickHandler& handler) {
        return ring_.consume_all([&handler](Tick&& tick) { handler(tick); });
    }

    /// Ticks discarded because the strategy fell behind the ring capacity.
    std::size_t dropped_ticks() const { return dropped_.load(std::memory_order_relaxed); }

    std::size_t queued_approx() const { return ring_.size_approx(); }

private:
    SPSCRingBuffer<Tick> ring_;
    TickSubscriber subscriber_;
    std::thread network_thread_;
    std::atomic<bool> running_{false};
    std::atomic<std::size_t> dropped_{0};
};

} // namespace qse
