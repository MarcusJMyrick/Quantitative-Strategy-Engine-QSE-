#include <iostream>
#include <memory>
#include "qse/data/CSVDataReader.h"
#include "qse/strategy/FillTrackingStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    std::cout << "=== DEBUG TEST STARTING ===" << std::endl;
    
    try {
        // Test with AAPL data
        std::string data_file = "data/raw_ticks_AAPL.csv";
        std::cout << "Loading data from: " << data_file << std::endl;
        
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        std::cout << "Data reader created successfully" << std::endl;
        
        auto order_manager = std::make_shared<qse::OrderManager>(
            100000.0, 
            "test_debug_equity.csv", 
            "test_debug_trades.csv"
        );
        std::cout << "Order manager created successfully" << std::endl;
        
        auto strategy = std::make_unique<qse::FillTrackingStrategy>(order_manager);
        std::cout << "FillTracking strategy created successfully" << std::endl;
        
        qse::Backtester backtester(
            "AAPL",
            std::move(data_reader),
            std::move(strategy),
            order_manager,
            std::chrono::seconds(60)
        );
        std::cout << "Backtester created successfully" << std::endl;
        
        std::cout << "Starting backtest..." << std::endl;
        backtester.run();
        std::cout << "Backtest completed successfully" << std::endl;
        
        // Check if files were created and have content
        std::ifstream equity_file("test_debug_equity.csv");
        std::ifstream trades_file("test_debug_trades.csv");
        
        if (equity_file.good()) {
            std::string line;
            int lines = 0;
            while (std::getline(equity_file, line)) lines++;
            std::cout << "Equity file created with " << lines << " lines" << std::endl;
        } else {
            std::cout << "WARNING: Equity file not created or empty" << std::endl;
        }
        
        if (trades_file.good()) {
            std::string line;
            int lines = 0;
            while (std::getline(trades_file, line)) lines++;
            std::cout << "Trades file created with " << lines << " lines" << std::endl;
        } else {
            std::cout << "WARNING: Trades file not created or empty" << std::endl;
        }
        
        std::cout << "=== DEBUG TEST COMPLETED ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
