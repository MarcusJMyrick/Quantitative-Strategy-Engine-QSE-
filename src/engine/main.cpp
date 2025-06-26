#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

#include "qse/data/CSVDataReader.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    try {
        std::cout << "Strategy Engine Starting..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // --- Configuration ---
        const std::string symbol = "AAPL";
        const std::string data_file = "data/raw_ticks_AAPL.csv";
        const double initial_capital = 100000.0;
        
        // --- Create Components ---
        std::cout << "Initializing components..." << std::endl;
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        auto order_manager = std::make_unique<qse::OrderManager>(
            initial_capital, 
            "equity_curve.csv", 
            "tradelog.csv"
        );
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(), 20, 50, symbol
        );
        
        qse::Backtester backtester(
            symbol,
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager),
            std::chrono::seconds(60)  // 1-minute bars
        );
        
        // --- Run Backtest ---
        std::cout << "Running backtest..." << std::endl;
        backtester.run();
        
        // --- Performance Results ---
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Backtest completed successfully!" << std::endl;
        std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;
        std::cout << "Performance: ~" << (19185.0 / (duration.count() / 1000.0)) << " ticks/second" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 