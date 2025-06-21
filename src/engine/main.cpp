#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

#include "qse/data/ZeroMQDataReader.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    try {
        std::cout << "--- Strategy Engine Starting ---" << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // --- Configuration ---
        const std::string symbol = "SPY"; // The symbol this engine instance will trade
        const std::string zmq_endpoint = "tcp://localhost:5555";
        const double initial_capital = 100000.0;
        
        // --- Create Components ---
        // The engine now subscribes to the data feed instead of reading a file.
        auto data_reader = std::make_unique<qse::ZeroMQDataReader>(zmq_endpoint);
        
        // Updated OrderManager constructor with file paths
        auto order_manager = std::make_unique<qse::OrderManager>(
            initial_capital, 
            "equity_curve.csv", 
            "tradelog.csv"
        );

        // This strategy will build 1-hour bars from the incoming tick stream
        auto strategy = std::make_unique<qse::SMACrossoverStrategy>(
            order_manager.get(), 10, 20, std::chrono::minutes(60)
        );

        // --- Create and Run the Backtester ---
        qse::Backtester backtester(
            symbol,
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );

        backtester.run();

        // --- Post-Run Reporting ---
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total execution time: " << elapsed_time.count() << " ms" << std::endl;
        std::cout << "Final Cash: $" << order_manager->get_cash() << std::endl;
        std::cout << "Final Position: " << order_manager->get_position(symbol) << " shares" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "--- Strategy Engine Finished ---" << std::endl;

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "An unhandled error occurred in main: " << e.what() << std::endl;
        return 1;
    }
} 