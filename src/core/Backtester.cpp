#include "qse/core/Backtester.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <filesystem>
#include <sstream>
#include <map>
#include <algorithm>

namespace qse {

// Constructor updated to initialize BarBuilder and OrderBook
Backtester::Backtester(
    const std::string& symbol,
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::shared_ptr<IOrderManager> order_manager,
    const std::chrono::seconds& bar_interval)
    : symbol_(symbol),
      strategy_(std::move(strategy)),
      order_manager_(order_manager),
      bar_builder_(bar_interval)
{
    if (data_reader) {
        data_readers_.push_back(std::move(data_reader));
    }
}

void Backtester::add_data_source(std::unique_ptr<IDataReader> data_reader)
{
    if (data_reader) {
        data_readers_.push_back(std::move(data_reader));
    }
}

void Backtester::run() {
    std::cout << "[Backtester] Starting backtest for " << symbol_ << "..." << std::endl;

    std::vector<Tick> all_ticks;
    for (const auto& reader : data_readers_) {
        const auto& ticks = reader->read_all_ticks();
        all_ticks.insert(all_ticks.end(), ticks.begin(), ticks.end());
    }

    if (all_ticks.empty()) {
        std::cout << "[Backtester] No ticks to process for " << symbol_ << "." << std::endl;
        return;
    }
    
    std::cout << "[Backtester] Processing " << all_ticks.size() << " ticks for " << symbol_ << std::endl;
    
    std::sort(all_ticks.begin(), all_ticks.end(), [](const Tick& a, const Tick& b) {
        return a.timestamp < b.timestamp;
    });

    for (const auto& tick : all_ticks) {
        // The strategy's on_tick is the primary event handler
        strategy_->on_tick(tick);

        // The bar builder aggregates ticks into bars and calls the strategy's on_bar via a callback when a new bar is formed
        if (auto bar = bar_builder_.add_tick(tick)) {
             strategy_->on_bar(*bar);
        }
    }
    
    // Flush any final bar
    if (auto bar = bar_builder_.flush()) {
        strategy_->on_bar(*bar);
    }
}

} // namespace qse
