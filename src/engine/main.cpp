#include <iostream>
#include <memory>
#include <string>
#include "qse/core/Backtester.h"
#include "qse/data/ZeroMQDataReader.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"

using namespace qse;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <zmq_endpoint>" << std::endl;
        std::cout << "Example: " << argv[0] << " tcp://localhost:5555" << std::endl;
        return 1;
    }

    std::string zmq_endpoint = argv[1];
    
    try {
        std::cout << "Starting Strategy Engine..." << std::endl;
        std::cout << "Connecting to data publisher at: " << zmq_endpoint << std::endl;
        
        // Create ZeroMQ data reader instead of CSV reader
        auto data_reader = std::make_unique<ZeroMQDataReader>(zmq_endpoint);
        
        // Create order manager with initial capital
        auto order_manager = std::make_unique<OrderManager>(100000.0, 1.0, 0.01);
        
        // Create strategy with moving average parameters
        auto strategy = std::make_unique<SMACrossoverStrategy>(order_manager.get(), 10, 20);
        
        // Create backtester
        Backtester backtester(
            "AAPL",  // Symbol
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );
        
        std::cout << "Running backtest with real-time data..." << std::endl;
        
        // Run the backtest
        backtester.run();
        
        std::cout << "Backtest completed successfully!" << std::endl;
        
        // Print final results
        std::cout << "\n=== Final Results ===" << std::endl;
        std::cout << "Final Portfolio Value: $" << order_manager->get_portfolio_value("AAPL") << std::endl;
        std::cout << "Final Position: " << order_manager->get_position("AAPL") << " shares" << std::endl;
        
        const auto& trade_log = order_manager->get_trade_log();
        std::cout << "Total Trades: " << trade_log.size() << std::endl;
        
        if (!trade_log.empty()) {
            std::cout << "\n=== Trade Summary ===" << std::endl;
            for (const auto& trade : trade_log) {
                std::cout << "Trade: " << (trade.type == TradeType::BUY ? "BUY" : "SELL")
                          << " " << trade.quantity << " shares at $" << trade.price
                          << " (Commission: $" << trade.commission << ")" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 