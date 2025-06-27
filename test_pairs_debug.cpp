#include <iostream>
#include <memory>
#include "qse/data/CSVDataReader.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    std::cout << "=== PairsTrading Debug Test ===" << std::endl;
    
    try {
        // Test with AAPL and GOOG data
        std::string data_file1 = "data/raw_ticks_AAPL.csv";
        std::string data_file2 = "data/raw_ticks_GOOG.csv";
        
        std::cout << "Loading data from: " << data_file1 << " and " << data_file2 << std::endl;
        
        auto order_manager = std::make_shared<qse::OrderManager>(
            100000.0, 
            "test_pairs_equity.csv", 
            "test_pairs_trades.csv"
        );
        std::cout << "Order manager created successfully" << std::endl;
        
        // Create PairsTrading strategy with more aggressive parameters
        auto strategy = std::make_unique<qse::PairsTradingStrategy>(
            "AAPL", "GOOG", 1.0, 10, 1.0, 0.2, order_manager);
        std::cout << "PairsTrading strategy created successfully" << std::endl;
        
        // Create backtester with AAPL data (primary source)
        qse::Backtester backtester(
            "AAPL_GOOG",
            std::make_unique<qse::CSVDataReader>(data_file1),
            std::move(strategy),
            order_manager,
            std::chrono::seconds(60)
        );
        std::cout << "Backtester created successfully" << std::endl;
        
        std::cout << "Starting PairsTrading backtest..." << std::endl;
        backtester.run();
        std::cout << "PairsTrading backtest completed" << std::endl;
        
        // Check results
        std::ifstream trades_file("test_pairs_trades.csv");
        if (trades_file.good()) {
            std::string line;
            int lines = 0;
            while (std::getline(trades_file, line)) lines++;
            std::cout << "Trades file created with " << lines << " lines" << std::endl;
            
            if (lines > 1) {
                std::cout << "Sample trades:" << std::endl;
                std::ifstream sample_file("test_pairs_trades.csv");
                std::string sample_line;
                for (int i = 0; i < std::min(5, lines); i++) {
                    std::getline(sample_file, sample_line);
                    std::cout << "  " << sample_line << std::endl;
                }
            }
        } else {
            std::cout << "WARNING: Trades file not created or empty" << std::endl;
        }
        
        std::cout << "=== PairsTrading Debug Test Complete ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
