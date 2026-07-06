#include "qse/live/LiveEngine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

namespace qse {

LiveEngine::LiveEngine(LiveEngineConfig config, IExecutionHandler& exec, DrainFn drain)
    : config_(std::move(config)), exec_(exec), drain_(std::move(drain)),
      bar_builder_(config_.bar_interval), short_ma_(config_.sma_short), long_ma_(config_.sma_long) {
    exec_.set_fill_callback([this](const Fill& fill) {
        fills_.push_back(fill);
        std::cout << "[LiveEngine] fill: " << fill.side << " " << fill.quantity << " "
                  << fill.symbol << " @ " << fill.price << " (order " << fill.order_id << ")"
                  << std::endl;
    });
}

std::size_t LiveEngine::step() {
    std::size_t processed = drain_([this](const Tick& tick) { on_tick(tick); });
    exec_.poll_fills();
    return processed;
}

void LiveEngine::run_for(std::chrono::seconds duration, std::chrono::milliseconds cadence) {
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + duration;
    auto next_heartbeat = start + std::chrono::seconds(30);
    std::size_t ticks_total = 0;

    while (running_.load(std::memory_order_relaxed) &&
           std::chrono::steady_clock::now() < deadline) {
        ticks_total += step();
        // Periodic proof of life: the loop is otherwise silent between
        // crossovers, which makes a healthy session look like a hung one
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_heartbeat) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            std::cout << "[LiveEngine] " << elapsed << "s elapsed: " << ticks_total << " ticks, "
                      << bars_seen_ << " bars, " << submitted_.size() << " orders, "
                      << fills_.size() << " fills"
                      << (ticks_total == 0 ? " (no fresh quotes - market closed?)" : "")
                      << std::endl;
            next_heartbeat = now + std::chrono::seconds(30);
        }
        std::this_thread::sleep_for(cadence);
    }
    // Final sweep so fills that landed during the last cadence are recorded
    step();
}

void LiveEngine::on_tick(const Tick& tick) {
    if (tick.symbol != config_.symbol) {
        return;
    }
    if (auto bar = bar_builder_.add_tick(tick)) {
        on_bar(*bar);
    }
}

void LiveEngine::on_bar(const Bar& bar) {
    ++bars_seen_;
    const double prev_short = short_ma_.get_value();
    const double prev_long = long_ma_.get_value();
    short_ma_.update(bar.close);
    long_ma_.update(bar.close);
    if (!long_ma_.is_ready() || prev_long <= 0.0) {
        return;
    }
    const double cur_short = short_ma_.get_value();
    const double cur_long = long_ma_.get_value();

    OrderId id;
    if (prev_short < prev_long && cur_short > cur_long && !is_long_) {
        std::cout << "[LiveEngine] golden cross @ " << bar.close << " - buying "
                  << config_.order_size << std::endl;
        id = exec_.submit_market_order(config_.symbol, Order::Side::BUY, config_.order_size);
        is_long_ = true;
    } else if (prev_short > prev_long && cur_short < cur_long && is_long_) {
        std::cout << "[LiveEngine] death cross @ " << bar.close << " - selling "
                  << config_.order_size << std::endl;
        id = exec_.submit_market_order(config_.symbol, Order::Side::SELL, config_.order_size);
        is_long_ = false;
    }
    if (!id.empty()) {
        submitted_.push_back(id);
    }
}

void LiveEngine::save_logs(const std::string& directory) const {
    std::filesystem::create_directories(directory);

    std::ofstream orders(directory + "/orders.csv");
    orders << "order_id\n";
    for (const auto& id : submitted_) {
        orders << id << "\n";
    }

    std::ofstream fills(directory + "/fills.csv");
    fills << "order_id,symbol,side,quantity,price\n";
    for (const auto& fill : fills_) {
        fills << fill.order_id << "," << fill.symbol << "," << fill.side << "," << fill.quantity
              << "," << fill.price << "\n";
    }
}

LiveEngine::Reconciliation LiveEngine::reconcile() const {
    Reconciliation report;
    for (const auto& id : submitted_) {
        ReconciliationLine line;
        line.order_id = id;
        for (const auto& fill : fills_) {
            if (fill.order_id == id) {
                line.local_fill_qty += fill.quantity;
            }
        }
        if (auto order = exec_.get_order(id)) {
            line.venue_reachable = true;
            line.venue_fill_qty = order->filled_quantity;
        }
        line.matched = line.venue_reachable && line.venue_fill_qty == line.local_fill_qty;
        report.all_matched = report.all_matched && line.matched;
        report.lines.push_back(line);
    }
    return report;
}

} // namespace qse
