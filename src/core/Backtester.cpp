#include "qse/core/Backtester.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <filesystem>
#include <sstream>

namespace qse {

// Constructor updated to initialize BarBuilder and OrderBook
Backtester::Backtester(
    const std::string& symbol,
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::unique_ptr<IOrderManager> order_manager,
    const std::chrono::seconds& bar_interval)
    : symbol_(symbol),
      data_reader_(std::move(data_reader)),
      strategy_(std::move(strategy)),
      order_manager_(std::move(order_manager)),
      bar_builder_(bar_interval),
      order_book_()
{}

void Backtester::run() {
    std::cout << "--- Starting Tick-Driven Backtester for " << symbol_ << " ---" << std::endl;
    
    // Set up fill callback to notify strategy of fills
    order_manager_->set_fill_callback([this](const qse::Fill& fill) {
        strategy_->on_fill(fill);
    });
    
    // Read all ticks from the data reader
    const std::vector<Tick>& all_ticks = data_reader_->read_all_ticks();
    
    std::cout << "Processing " << all_ticks.size() << " ticks for " << symbol_ << "..." << std::endl;
    if (all_ticks.empty()) {
        std::cout << "No tick data to process for " << symbol_ << ". Exiting." << std::endl;
        return;
    }
    
    int tick_count = 0;
    int bar_count = 0;
    
    // Main tick-driven loop
    for (const auto& tick : all_ticks) {
        tick_count++;
        
        // Update order book with new tick
        order_book_.on_tick(tick);
        
        // Build bars from ticks
        if (auto bar = bar_builder_.add_tick(tick)) {
            bar_count++;
            strategy_->on_bar(*bar);
        }
        
        // Notify strategy of tick
        strategy_->on_tick(tick);
        
        // Attempt fills against current order book
        order_manager_->attempt_fills();
        
        // Process tick for legacy order management
        order_manager_->process_tick(tick);
    }
    
    // Flush any remaining bars
    if (auto bar = bar_builder_.flush()) {
        bar_count++;
        strategy_->on_bar(*bar);
    }
    
    std::cout << "--- Backtest Complete ---" << std::endl;
    std::cout << "Ticks processed: " << tick_count << std::endl;
    std::cout << "Bars created: " << bar_count << std::endl;
    std::cout << "Final cash: $" << order_manager_->get_cash() << std::endl;
    std::cout << "Final position: " << order_manager_->get_position(symbol_) << std::endl;
}

} // namespace qse
