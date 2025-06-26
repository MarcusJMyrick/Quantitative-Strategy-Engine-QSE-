#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include "qse/data/CSVDataReader.h"
#include "qse/order/OrderManager.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/core/Backtester.h"

int main() {
    try {
        std::cout << "=== Starting Direct Test ===" << std::endl;
        
        // Test 1: Basic string operations
        std::cout << "Test 1: Basic string operations..." << std::endl;
        const std::string symbol = "AAPL";
        const std::string data_file = "data/raw_ticks_AAPL.csv";
        const double initial_capital = 100000.0;
        std::cout << "Test 1: PASSED" << std::endl;
        
        // Test 2: Check if data file exists
        std::cout << "Test 2: Checking data file..." << std::endl;
        std::ifstream file(data_file);
        if (!file.is_open()) {
            std::cerr << "ERROR: Cannot open data file: " << data_file << std::endl;
            return 1;
        }
        file.close();
        std::cout << "Test 2: PASSED" << std::endl;
        
        // Test 3: Construct CSVDataReader
        std::cout << "Test 3: Constructing CSVDataReader..." << std::endl;
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        std::cout << "Test 3: PASSED (CSVDataReader constructed)" << std::endl;
        
        // Test 4: Construct OrderManager
        std::cout << "Test 4: Constructing OrderManager..." << std::endl;
        auto order_manager = std::make_unique<qse::OrderManager>(
            initial_capital, "equity_curve.csv", "tradelog.csv");
        std::cout << "Test 4: PASSED (OrderManager constructed)" << std::endl;
        
        // Test 5: Construct SMACrossoverStrategy
        std::cout << "Test 5: Constructing SMACrossoverStrategy..." << std::endl;
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(), 10, 20, symbol);
        std::cout << "Test 5: PASSED (SMACrossoverStrategy constructed)" << std::endl;
        
        // Test 6: Construct Backtester
        std::cout << "Test 6: Constructing Backtester..." << std::endl;
        qse::Backtester backtester(
            symbol,
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );
        std::cout << "Test 6: PASSED (Backtester constructed)" << std::endl;
        
        std::cout << "=== All Component Construction Tests Passed ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in direct test: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in direct test" << std::endl;
        return 1;
    }
} 