#include "Backtester.h"
#include "CSVDataReader.h" // Use the existing CSV reader for now
#include "OrderManager.h"
#include "SMACrossoverStrategy.h"
#include <memory>
#include <iostream>
#include <chrono>
#include <stdexcept>

int main() {
    try {
        // --- Configuration ---
        // Note: Using the existing CSV reader and data file.
        // The switch to a Parquet reader would happen in a later phase.
        const std::string data_path = "data/raw_SPY.csv";
        const double starting_cash = 100000.0;
        const double commission_per_trade = 1.0;
        const double slippage_per_trade = 0.01;
        const int short_window = 10;
        const int long_window = 30;

        // --- Dependency Injection ---
        // Create the components required for the backtest.
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_path);

        auto order_manager = std::make_unique<qse::OrderManager>(
            starting_cash,
            commission_per_trade,
            slippage_per_trade
        );

        // The strategy needs a raw pointer to the OrderManager to execute trades,
        // but the Backtester will maintain ownership of the unique_ptr.
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(),
            short_window,
            long_window
        );

        // --- Backtester Setup ---
        // The Backtester takes ownership of the components.
        qse::Backtester backtester(
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );
        
        // --- BENCHMARKING ---
        // Measure the execution time of the backtest run.
        auto start_time = std::chrono::high_resolution_clock::now();

        backtester.run();

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Backtest executed in: " << elapsed_time.count() << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

    } catch (const std::exception& e) {
        // Catch any standard exceptions (e.g., file not found) and report the error.
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1; // Indicate failure
    }
    
    return 0; // Indicate success
}