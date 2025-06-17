#include "CSVDataReader.h"
#include "OrderManager.h"
#include "Backtester.h"
#include "SMACrossoverStrategy.h"
#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <filesystem>

namespace fs = std::filesystem;

// Configuration for a single backtest
struct BacktestConfig {
    std::string symbol;
    double initial_capital;
    int short_window;
    int long_window;
    double transaction_cost;
};

// Function to run a single backtest
void run_backtest(const BacktestConfig& config) {
    try {
        // Create data reader
        std::string data_path = "data/raw_" + config.symbol + ".csv";
        qse::CSVDataReader reader(data_path);
        
        // Create order manager
        auto order_manager = std::make_unique<qse::OrderManager>(
            config.initial_capital,
            config.transaction_cost,
            0.0  // slippage
        );
        
        // Create strategy
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(),
            config.short_window,
            config.long_window
        );
        
        // Create backtester
        qse::Backtester backtester(
            std::make_unique<qse::CSVDataReader>(data_path),
            std::move(strategy),
            std::move(order_manager)
        );
        
        // Run backtest
        std::cout << "Running backtest for " << config.symbol << "..." << std::endl;
        backtester.run();
        
        // Get the last bar's close price for portfolio value calculation
        auto bars = reader.read_bars_in_range(
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max()
        );
        double last_price = bars.empty() ? 0.0 : bars.back().close;
        
        // Print results
        std::cout << "Results for " << config.symbol << ":" << std::endl;
        std::cout << "Final Position: " << order_manager->get_position() << std::endl;
        std::cout << "Final Portfolio Value: " << order_manager->get_portfolio_value(last_price) << std::endl;
        std::cout << "------------------------" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in backtest for " << config.symbol << ": " << e.what() << std::endl;
    }
}

int main() {
    try {
        // Create data directory if it doesn't exist
        fs::create_directories("data");
        
        // Define backtest configurations
        std::vector<BacktestConfig> configs = {
            {"SPY", 100000.0, 20, 50, 0.001},
            {"AAPL", 100000.0, 20, 50, 0.001},
            {"GOOG", 100000.0, 20, 50, 0.001}
        };
        
        // Create thread pool with number of threads equal to number of configs
        qse::ThreadPool pool(configs.size());
        
        // Vector to store futures
        std::vector<std::future<void>> futures;
        
        // Enqueue backtests
        for (const auto& config : configs) {
            futures.push_back(
                pool.enqueue([config]() {
                    run_backtest(config);
                })
            );
        }
        
        // Wait for all backtests to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
}