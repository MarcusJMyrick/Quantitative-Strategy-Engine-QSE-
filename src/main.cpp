#include "Backtester.h"
#include "DataReader.h" // Corrected: Use DataReader, not CSVDataReader
#include "OrderManager.h"
#include "SMACrossoverStrategy.h"
#include <memory>
#include <iostream>
#include <chrono>

int main() {
    try {
        // Create components with realistic transaction costs
        auto data_reader = std::make_unique<qse::DataReader>("data/SPY.parquet");
        auto order_manager = std::make_unique<qse::OrderManager>(
            100000.0,  // Starting cash
            1.0,       // Commission per trade ($1)
            0.01       // Slippage (1 cent per share)
        );
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(order_manager.get(), 10, 30);

        // Create and run the backtester
        qse::Backtester backtester(
            std::move(data_reader),
            std::move(strategy),
            std::unique_ptr<qse::IOrderManager>(std::move(order_manager))
        );
        
        // --- BENCHMARKING CODE ---
        auto start = std::chrono::high_resolution_clock::now();

        backtester.run();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Backtest executed in: " << elapsed.count() << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        // -------------------------

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}