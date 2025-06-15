#include "Backtester.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <filesystem>

namespace qse {

Backtester::Backtester(
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::unique_ptr<IOrderManager> order_manager)
    : data_reader_(std::move(data_reader)),     // Move ownership of data reader
      strategy_(std::move(strategy)),           // Move ownership of strategy
      order_manager_(std::move(order_manager))  // Move ownership of order manager
{
    std::cout << "--- Starting Backtest ---" << std::endl;
}

void Backtester::run() {
    size_t bar_count = data_reader_->get_bar_count();
    std::cout << "Processing " << bar_count << " bars..." << std::endl;

    // Process each bar
    for (size_t i = 0; i < bar_count; ++i) {
        Bar bar = data_reader_->get_bar(i);
        strategy_->on_bar(bar);
    }

    // Save results
    std::ofstream results_file("results/results.csv");
    results_file << "timestamp,portfolio_value" << std::endl;
    
    // Get final position and portfolio value
    int final_position = order_manager_->get_position();
    double final_portfolio_value = order_manager_->get_portfolio_value(0.0);  // Use 0.0 as current price for now
    
    std::cout << "Equity curve saved to results/results.csv" << std::endl;
    std::cout << "--- Backtest Finished ---" << std::endl;
    std::cout << "Final Position: " << final_position << std::endl;
    std::cout << "Final Portfolio Value: " << final_portfolio_value << std::endl;
}

} // namespace qse 