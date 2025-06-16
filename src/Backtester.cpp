#include "Backtester.h"
#include <iostream>
#include <fstream>      // Required for file writing
#include <vector>
#include <utility>      // Required for std::pair
#include <filesystem>   // Required for creating directories
#include <sstream>      // <-- Added for optimized string building

namespace qse {

Backtester::Backtester(
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::unique_ptr<IOrderManager> order_manager)
    : data_reader_(std::move(data_reader)),
      strategy_(std::move(strategy)),
      order_manager_(std::move(order_manager))
{}

void Backtester::run() {
    std::cout << "--- Starting Backtest ---" << std::endl;
      
    std::vector<Bar> all_data = data_reader_->read_all_bars();
    std::cout << "Processing " << all_data.size() << " bars..." << std::endl;

    if (all_data.empty()) {
        std::cout << "No data to process. Exiting." << std::endl;
        return;
    }

    // A vector to store our equity history
    std::vector<std::pair<Timestamp, double>> equity_curve;

    for (const auto& bar : all_data) {
        strategy_->on_bar(bar);
          
        // After each bar, record the portfolio's total value
        double current_value = order_manager_->get_portfolio_value(bar.close);
        equity_curve.emplace_back(bar.timestamp, current_value);
    }
      
    // --- Create results directory and write output files ---
    std::filesystem::create_directories("results");

    // --- Write Equity Curve Log ---
    std::ofstream equity_file("results/equity.csv");
    equity_file << "timestamp,portfolio_value\n"; // CSV Header
    for (const auto& record : equity_curve) {
        // Convert time_point to seconds since epoch for easy parsing in Python
        auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(record.first);
        equity_file << seconds.time_since_epoch().count() << "," << record.second << "\n";
    }
    equity_file.close();
    std::cout << "Equity curve saved to results/equity.csv" << std::endl;

    // --- Optimized Trade Log Writing ---
    std::stringstream trade_log_ss;
    trade_log_ss << "timestamp,type,price,quantity,commission\n"; // CSV Header
    const auto& trade_log = order_manager_->get_trade_log();
    for (const auto& trade : trade_log) {
        auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(trade.timestamp);
        trade_log_ss << seconds.time_since_epoch().count() << ","
                     << (trade.type == TradeType::BUY ? "BUY" : "SELL") << ","
                     << trade.price << "," << trade.quantity << ","
                     << trade.commission << "\n";
    }

    // Write the entire stringstream to the file in one operation
    std::ofstream trade_log_file("results/tradelog.csv");
    trade_log_file << trade_log_ss.str();
    trade_log_file.close();
    std::cout << "Trade log saved to results/tradelog.csv" << std::endl;


    std::cout << "--- Backtest Finished ---" << std::endl;
    std::cout << "Final Position: " << order_manager_->get_position() << std::endl;
    // Correctly calculate final value using the last known price
    std::cout << "Final Portfolio Value: " << order_manager_->get_portfolio_value(all_data.back().close) << std::endl;
}

} // namespace qse