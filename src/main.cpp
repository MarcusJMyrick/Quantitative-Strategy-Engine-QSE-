#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <future>
#include <filesystem>
#include <stdexcept>
#include <chrono>

#include "ThreadPool.h"
#include "CSVDataReader.h"
#include "SMACrossoverStrategy.h"
#include "OrderManager.h"
#include "Backtester.h"

struct BacktestConfig {
    std::string symbol;
    std::string data_path;
    double initial_capital;
    int short_window;
    int long_window;
    double commission;
    double slippage;
};

void run_backtest(const BacktestConfig& config) {
    try {
        auto data_reader = std::make_unique<qse::CSVDataReader>(config.data_path);
        auto order_manager = std::make_unique<qse::OrderManager>(
            config.initial_capital, config.commission, config.slippage
        );
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(), config.short_window, config.long_window
        );

        // --- FIX: Pass the symbol from the config into the Backtester constructor ---
        qse::Backtester backtester(
            config.symbol, // <-- The missing argument
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );

        backtester.run();
        
    } catch (const std::exception& e) {
        std::cerr << "!!! Error in backtest for " << config.symbol << ": " << e.what() << std::endl;
    }
}

int main() {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::filesystem::create_directories("data");
        
        std::vector<BacktestConfig> configs = {
            {"SPY",  "data/raw_SPY.csv",  100000.0, 20, 50, 1.0, 0.01},
            {"AAPL", "data/raw_AAPL.csv", 100000.0, 15, 45, 1.0, 0.01},
            {"GOOG", "data/raw_GOOG.csv", 100000.0, 50, 200, 1.0, 0.01},
            {"MSFT", "data/raw_MSFT.csv", 250000.0, 10, 30, 1.0, 0.01}
        };
        
        qse::ThreadPool pool(configs.size());
        std::cout << "ThreadPool created with " << configs.size() << " threads." << std::endl;
        
        std::vector<std::future<void>> futures;
        
        for (const auto& config : configs) {
            futures.emplace_back(pool.enqueue([config] { run_backtest(config); }));
        }
        
        std::cout << "\nAll backtests enqueued. Waiting for completion..." << std::endl;
        for (auto& future : futures) {
            future.get();
        }
        
        std::cout << "\nAll backtests have completed successfully." << std::endl;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total execution time for all backtests: " << elapsed_time.count() << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "An unhandled error occurred in main: " << e.what() << std::endl;
        return 1;
    }
}
