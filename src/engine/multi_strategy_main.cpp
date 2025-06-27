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
#include "qse/strategy/FillTrackingStrategy.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "qse/strategy/DoNothingStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

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

void run_all_strategies_for_symbol(const std::string& symbol, const std::string& timestamp_suffix) {
    try {
        std::cout << "\n=== Running All Strategies for " << symbol << " ===" << std::endl;
        
        // Configuration
        const std::string data_file = "data/raw_ticks_" + symbol + ".csv";
        const double initial_capital = 100000.0;
        
        std::cout << "Data file: " << data_file << std::endl;
        
        // ---------- Strategy 1: SMA Crossover (20/50) ----------
        {
            std::cout << "Running SMA Crossover Strategy..." << std::endl;
            auto om_sma = std::make_unique<qse::OrderManager>(
                initial_capital,
                "results/equity_" + symbol + "_SMA_20_50_" + timestamp_suffix + ".csv",
                "results/tradelog_" + symbol + "_SMA_20_50_" + timestamp_suffix + ".csv");

            auto strat_sma = std::make_unique<qse::SMACrossoverStrategy>(
                om_sma.get(), 20, 50, symbol);

            qse::Backtester bt_sma(
                symbol,
                std::make_unique<qse::CSVDataReader>(data_file),
                std::move(strat_sma),
                std::move(om_sma),
                std::chrono::seconds(60));

            bt_sma.run();
        }

        // ---------- Strategy 2: Fill Tracking (Smoke Test) ----------
        {
            std::cout << "Running Fill Tracking Strategy..." << std::endl;
            auto om_fill = std::make_shared<qse::OrderManager>(
                initial_capital,
                "results/equity_" + symbol + "_FillTracking_" + timestamp_suffix + ".csv",
                "results/tradelog_" + symbol + "_FillTracking_" + timestamp_suffix + ".csv");

            auto strat_fill = std::make_unique<qse::FillTrackingStrategy>(om_fill);

            qse::Backtester bt_fill(
                symbol,
                std::make_unique<qse::CSVDataReader>(data_file),
                std::move(strat_fill),
                om_fill,
                std::chrono::seconds(60));

            bt_fill.run();
        }

        // ---------- Strategy 3: Do Nothing (Baseline) ----------
        {
            std::cout << "Running Do Nothing Strategy (Baseline)..." << std::endl;
            auto om_do_nothing = std::make_unique<qse::OrderManager>(
                initial_capital,
                "results/equity_" + symbol + "_DoNothing_" + timestamp_suffix + ".csv",
                "results/tradelog_" + symbol + "_DoNothing_" + timestamp_suffix + ".csv");

            auto strat_do_nothing = std::make_unique<qse::DoNothingStrategy>();

            qse::Backtester bt_do_nothing(
                symbol,
                std::make_unique<qse::CSVDataReader>(data_file),
                std::move(strat_do_nothing),
                std::move(om_do_nothing),
                std::chrono::seconds(60));

            bt_do_nothing.run();
        }

        std::cout << "Completed " << symbol << " with all individual strategies" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error running strategies for " << symbol << ": " << e.what() << std::endl;
    }
}

void run_pairs_trading(const std::string& timestamp_suffix) {
    try {
        std::cout << "\n=== Running Pairs Trading Strategy (Aggressive) ===" << std::endl;
        
        // Pairs trading needs two symbols - using AAPL and GOOG as an example
        const std::string symbol1 = "AAPL";
        const std::string symbol2 = "GOOG";
        const std::string data_file1 = "data/raw_ticks_" + symbol1 + ".csv";
        const std::string data_file2 = "data/raw_ticks_" + symbol2 + ".csv";
        const double initial_capital = 100000.0;
        
        std::cout << "Pairs Trading: " << symbol1 << " vs " << symbol2 << std::endl;
        std::cout << "Using AGGRESSIVE parameters:" << std::endl;
        std::cout << "  - Spread window: 10 (was 20)" << std::endl;
        std::cout << "  - Entry threshold: 1.0 (was 2.0)" << std::endl;
        std::cout << "  - Exit threshold: 0.2 (was 0.5)" << std::endl;
        
        auto om_pairs = std::make_shared<qse::OrderManager>(
            initial_capital,
            "results/equity_PairsTrading_" + symbol1 + "_" + symbol2 + "_" + timestamp_suffix + ".csv",
            "results/tradelog_PairsTrading_" + symbol1 + "_" + symbol2 + "_" + timestamp_suffix + ".csv");

        // Use aggressive parameters that should generate trades
        auto strat_pairs = std::make_unique<qse::PairsTradingStrategy>(
            symbol1, symbol2, 1.0, 10, 1.0, 0.2, om_pairs);

        qse::Backtester bt_pairs(
            symbol1 + "_" + symbol2, // Combined symbol name
            std::make_unique<qse::CSVDataReader>(data_file1, symbol1), // Primary data source
            std::move(strat_pairs),
            om_pairs,
            std::chrono::seconds(60));

        // Add the second leg's data source so the backtester processes both streams
        bt_pairs.add_data_source(std::make_unique<qse::CSVDataReader>(data_file2, symbol2));

        bt_pairs.run();
        
        std::cout << "Completed Pairs Trading Strategy (Aggressive)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error running pairs trading: " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== Multi-Strategy Engine Starting ===" << std::endl;
        auto overall_start = std::chrono::high_resolution_clock::now();
        
        // Generate timestamp suffix for this run
        std::string timestamp_suffix = get_timestamp_suffix();
        std::cout << "Run timestamp: " << timestamp_suffix << std::endl;
        
        // List of symbols to process
        std::vector<std::string> symbols = {"AAPL", "GOOG", "MSFT", "SPY"};
        
        // Run all individual strategies for each symbol
        for (const auto& symbol : symbols) {
            run_all_strategies_for_symbol(symbol, timestamp_suffix);
        }
        
        // Run pairs trading strategy
        run_pairs_trading(timestamp_suffix);
        
        // Overall timing
        auto overall_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(overall_end - overall_start);
        
        std::cout << "\n=== Multi-Strategy Engine Complete ===" << std::endl;
        std::cout << "Total execution time: " << total_duration.count() << " ms" << std::endl;
        std::cout << "Results saved with timestamp: " << timestamp_suffix << std::endl;
        std::cout << "\nStrategies run:" << std::endl;
        std::cout << "  - SMA Crossover (20/50)" << std::endl;
        std::cout << "  - Fill Tracking (Smoke Test)" << std::endl;
        std::cout << "  - Do Nothing (Baseline)" << std::endl;
        std::cout << "  - Pairs Trading (AAPL vs GOOG)" << std::endl;
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "1. Run: python3 scripts/analysis/analyze_multi_strategy.py" << std::endl;
        std::cout << "2. Check plots/ directory for generated charts" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
} 