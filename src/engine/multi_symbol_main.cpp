#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "qse/data/CSVDataReader.h"
#include "qse/strategy/SMACrossoverStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"
#include "qse/strategy/FillTrackingStrategy.h"

std::string get_timestamp_suffix() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void run_strategy_for_symbol(const std::string& symbol, const std::string& timestamp_suffix) {
    try {
        std::cout << "\n=== Running Strategy for " << symbol << " ===" << std::endl;
        
        // Configuration
        const std::string data_file = "data/raw_ticks_" + symbol + ".csv";
        const double initial_capital = 100000.0;
        
        // Create unique output filenames with timestamp
        const std::string equity_file = "results/equity_" + symbol + "_" + timestamp_suffix + ".csv";
        const std::string tradelog_file = "results/tradelog_" + symbol + "_" + timestamp_suffix + ".csv";
        
        std::cout << "Data file: " << data_file << std::endl;
        std::cout << "Equity output: " << equity_file << std::endl;
        std::cout << "Tradelog output: " << tradelog_file << std::endl;
        
        // Shared data reader (can be reused by both strategies)
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);

        // ---------- Strategy 1: Long-term SMA (20/50) ----------
        {
            auto om_sma = std::make_unique<qse::OrderManager>(
                initial_capital,
                "results/equity_" + symbol + "_sma_" + timestamp_suffix + ".csv",
                "results/tradelog_" + symbol + "_sma_" + timestamp_suffix + ".csv");

            auto strat_sma = std::make_unique<qse::SMACrossoverStrategy>(
                om_sma.get(), 20, 50, symbol);

            qse::Backtester bt_sma(
                symbol,
                std::make_unique<qse::CSVDataReader>(data_file), // fresh reader
                std::move(strat_sma),
                std::move(om_sma),
                std::chrono::seconds(60));

            bt_sma.run();
        }

        // ---------- Strategy 2: Fill-tracking smoke test ----------
        {
            auto om_fill = std::make_shared<qse::OrderManager>(
                initial_capital,
                "results/equity_" + symbol + "_fill_" + timestamp_suffix + ".csv",
                "results/tradelog_" + symbol + "_fill_" + timestamp_suffix + ".csv");

            auto strat_fill = std::make_unique<qse::FillTrackingStrategy>(om_fill);

            qse::Backtester bt_fill(
                symbol,
                std::make_unique<qse::CSVDataReader>(data_file), // fresh reader
                std::move(strat_fill),
                om_fill,
                std::chrono::seconds(60));

            bt_fill.run();
        }

        std::cout << "Completed " << symbol << " with SMA & FillTracking" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error running strategy for " << symbol << ": " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== Multi-Symbol Strategy Engine Starting ===" << std::endl;
        auto overall_start = std::chrono::high_resolution_clock::now();
        
        // Generate timestamp suffix for this run
        std::string timestamp_suffix = get_timestamp_suffix();
        std::cout << "Run timestamp: " << timestamp_suffix << std::endl;
        
        // List of symbols to process
        std::vector<std::string> symbols = {"AAPL", "GOOG", "MSFT", "SPY"};
        
        // Run strategy for each symbol
        for (const auto& symbol : symbols) {
            run_strategy_for_symbol(symbol, timestamp_suffix);
        }
        
        // Overall timing
        auto overall_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(overall_end - overall_start);
        
        std::cout << "\n=== Multi-Symbol Strategy Engine Complete ===" << std::endl;
        std::cout << "Total execution time: " << total_duration.count() << " ms" << std::endl;
        std::cout << "Results saved with timestamp: " << timestamp_suffix << std::endl;
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "1. Run: python3 scripts/analyze_multi_run.py " << timestamp_suffix << std::endl;
        std::cout << "2. Check plots/ directory for generated charts" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
} 