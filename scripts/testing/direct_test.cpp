#include <iostream>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

#include "qse/data/CSVDataReader.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    try {
        std::cout << "--- Direct Performance Test ---" << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Configuration
        const std::string data_file = "test_data/test_raw_ticks_SPY.csv";
        const std::string symbol = "SPY";
        const double initial_capital = 100000.0;
        
        // Check if test data exists
        std::ifstream test_file(data_file);
        if (!test_file.good()) {
            std::cerr << "❌ Test data file not found: " << data_file << std::endl;
            std::cerr << "Run: python3 scripts/test_with_subset.py" << std::endl;
            return 1;
        }
        
        std::cout << "📊 Using test data: " << data_file << std::endl;
        
        // Create components (direct CSV reading, no ZeroMQ)
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        auto order_manager = std::make_unique<qse::OrderManager>(
            initial_capital, 
            "test_equity.csv", 
            "test_tradelog.csv"
        );
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(), 10, 20, symbol
        );

        // Create and run backtester
        qse::Backtester backtester(
            symbol,
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );

        std::cout << "🚀 Starting direct backtest..." << std::endl;
        backtester.run();

        // Performance reporting
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "⏱️  Performance Results:" << std::endl;
        std::cout << "Total execution time: " << elapsed_time.count() << " ms" << std::endl;
        std::cout << "Time per tick: " << (elapsed_time.count() / 1000.0) << " ms" << std::endl;
        std::cout << "Ticks per second: " << (1000.0 / (elapsed_time.count() / 1000.0)) << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "--- Direct Test Complete ---" << std::endl;

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
} 