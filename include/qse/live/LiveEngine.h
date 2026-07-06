#pragma once

#include "qse/data/BarBuilder.h"
#include "qse/data/Data.h"
#include "qse/exe/IExecutionHandler.h"
#include "qse/strategy/MovingAverage.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace qse {

struct LiveEngineConfig {
    std::string symbol = "AAPL";
    std::chrono::seconds bar_interval{60};
    std::size_t sma_short = 5;
    std::size_t sma_long = 20;
    Volume order_size = 1;
};

/**
 * @brief The live trading loop (E3): ticks in, orders out, books reconciled.
 *
 * Venue-agnostic by construction - it consumes ticks from any drain function
 * (the Alpaca REST feed, the ZeroMQ pipeline, or a test ring) and routes
 * orders through any IExecutionHandler (Alpaca paper or the simulated
 * backtest engine). Strategy: the SMA crossover, long/flat, one order per
 * signal - the same logic the backtests run.
 *
 * Every submitted order and every fill is recorded locally; reconcile()
 * queries the venue per order id and compares executed quantity against the
 * local fill log - the E3 done-when check.
 */
class LiveEngine {
public:
    using DrainFn = std::function<std::size_t(const std::function<void(const Tick&)>&)>;

    LiveEngine(LiveEngineConfig config, IExecutionHandler& exec, DrainFn drain);

    /// One loop iteration: drain pending ticks through the strategy, then
    /// poll the venue for fills. Returns the number of ticks processed.
    std::size_t step();

    /// Runs step() on the given cadence until the duration elapses or stop()
    /// is called from a signal handler.
    void run_for(std::chrono::seconds duration,
                 std::chrono::milliseconds cadence = std::chrono::milliseconds(250));

    void stop() { running_.store(false, std::memory_order_relaxed); }

    // --- Local books ---
    const std::vector<OrderId>& submitted_orders() const { return submitted_; }
    const std::vector<Fill>& local_fills() const { return fills_; }
    std::size_t bars_seen() const { return bars_seen_; }

    /// Writes orders.csv / fills.csv under the given directory.
    void save_logs(const std::string& directory) const;

    // --- Reconciliation (the done-when) ---
    struct ReconciliationLine {
        OrderId order_id;
        Volume local_fill_qty = 0;
        Volume venue_fill_qty = 0;
        bool venue_reachable = false;
        bool matched = false;
    };
    struct Reconciliation {
        std::vector<ReconciliationLine> lines;
        bool all_matched = true;
    };
    Reconciliation reconcile() const;

private:
    void on_tick(const Tick& tick);
    void on_bar(const Bar& bar);

    LiveEngineConfig config_;
    IExecutionHandler& exec_;
    DrainFn drain_;

    BarBuilder bar_builder_;
    MovingAverage short_ma_;
    MovingAverage long_ma_;
    bool is_long_ = false;
    std::size_t bars_seen_ = 0;

    std::vector<OrderId> submitted_;
    std::vector<Fill> fills_;
    std::atomic<bool> running_{true};
};

} // namespace qse
