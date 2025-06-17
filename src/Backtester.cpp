#include "Backtester.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <filesystem>
#include <sstream>

namespace qse {

// Constructor remains the same
Backtester::Backtester(
    const std::string& symbol,
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::unique_ptr<IOrderManager> order_manager)
    : symbol_(symbol),
      data_reader_(std::move(data_reader)),
      strategy_(std::move(strategy)),
      order_manager_(std::move(order_manager))
{}

void Backtester::run() {
    std::cout << "--- Starting Tick-Driven Backtest for " << symbol_ << " ---" << std::endl;
    
    // --- CHANGE: Read all TICKS instead of bars ---
    const std::vector<Tick>& all_ticks = data_reader_->read_all_ticks();
    
    std::cout << "Processing " << all_ticks.size() << " ticks for " << symbol_ << "..." << std::endl;
    if (all_ticks.empty()) {
        std::cout << "No tick data to process for " << symbol_ << ". Exiting." << std::endl;
        return;
    }

    // --- The main loop now iterates over TICKS ---
    for (const auto& tick : all_ticks) {
        // Feed each tick to the strategy.
        strategy_->on_tick(tick);
          
        // The equity curve logic would need to be updated to record value more frequently.
        // For now, we'll omit it to simplify the transition to a tick-based system.
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
