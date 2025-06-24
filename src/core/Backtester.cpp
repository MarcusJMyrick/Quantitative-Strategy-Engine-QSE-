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
    std::cout << "--- Starting Tick-Driven Backtest for " << symbol_ << " ---" << std::endl;
    
    // Read all ticks from the data reader
    const std::vector<Tick>& all_ticks = data_reader_->read_all_ticks();
    
    std::cout << "Processing " << all_ticks.size() << " ticks for " << symbol_ << "..." << std::endl;
    if (all_ticks.empty()) {
        std::cout << "No tick data to process for " << symbol_ << ". Exiting." << std::endl;
        return;
    }

    // --- NEW: The main tick-driven loop ---
    // This implements the architecture: Ticks → BarBuilder → Bars → Strategy
    for (const auto& tick : all_ticks) {
        // 1. Update the order book with the new tick
        order_book_.on_tick(tick);
        
        // 2. Feed tick to BarBuilder and get any completed bars
        if (auto bar = bar_builder_.add_tick(tick)) {
            // 3. If a bar was completed, feed it to the strategy
            strategy_->on_bar(*bar);
        }
        
        // 4. Feed the tick directly to the strategy (for tick-driven strategies)
        strategy_->on_tick(tick);
        
        // 5. Process the order book for fills
        order_manager_->process_tick(tick);
    }
    
    // Flush any remaining bars at the end
    while (auto bar = bar_builder_.flush()) {
        strategy_->on_bar(*bar);
    }
      
    std::filesystem::create_directories("results");
    
    // --- The results generation would also need to be updated ---
    // For now, we comment this part out as the equity curve is not being built
    // and the order manager is not being called in the same way.
    /*
    std::string equity_filename = "results/equity_" + symbol_ + ".csv";
    ...
    std::string tradelog_filename = "results/tradelog_" + symbol_ + ".csv";
    ...
    */

    std::cout << "--- Backtest Finished for " << symbol_ << " ---" << std::endl;
    
    // Reporting final value is more complex without a simple "last bar".
    // We will revisit more advanced reporting in a later phase.
}

} // namespace qse
