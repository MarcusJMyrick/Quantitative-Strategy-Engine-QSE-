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
    std::unique_ptr<OrderManager> order_manager
) : data_reader_(std::move(data_reader)),
    strategy_(std::move(strategy)),
    order_manager_(std::move(order_manager)) {}

void Backtester::run() {
    std::cout << "--- Starting Backtest ---" << std::endl;
    
    std::vector<Bar> all_data = data_reader_->read_all_bars();
    std::cout << "Processing " << all_data.size() << " bars..." << std::endl;

    // A vector to store our equity history
    std::vector<std::pair<Timestamp, double>> equity_curve;

    for (const auto& bar : all_data) {
        strategy_->on_bar(bar);
        
        // After each bar, record the portfolio's total value
        double current_value = order_manager_->get_portfolio_value(bar.close);
        equity_curve.emplace_back(bar.timestamp, current_value);
    }
    
    // --- Write results to a CSV file in results/ directory ---
    std::filesystem::create_directories("results");
    std::ofstream results_file("results/results.csv");
    results_file << "timestamp,portfolio_value\n"; // CSV Header
    for (const auto& record : equity_curve) {
        // Convert time_point to seconds since epoch for easy parsing in Python
        auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(record.first);
        results_file << seconds.time_since_epoch().count() << "," << record.second << "\n";
    }
    results_file.close();
    std::cout << "Equity curve saved to results/results.csv" << std::endl;

    std::cout << "--- Backtest Finished ---" << std::endl;
    std::cout << "Final Position: " << order_manager_->get_position() << std::endl;
    std::cout << "Final Portfolio Value: " << order_manager_->get_portfolio_value(all_data.back().close) << std::endl;
}

} // namespace qse 