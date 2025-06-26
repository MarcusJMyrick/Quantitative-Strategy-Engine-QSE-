#include "qse/core/Backtester.h"
#include "qse/core/Config.h"
#include "qse/data/CSVDataReader.h"
#include "qse/order/OrderManager.h"
#include "qse/strategy/FillTrackingStrategy.h"
#include "qse/strategy/PairsTradingStrategy.h"
#include "qse/strategy/SMACrossoverStrategy.h"

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <chrono>

using namespace qse;

void run_minimal_test() {
    std::cerr << "=== MINIMAL TEST STARTING ===" << std::endl;

    // Step 1: Create a shared OrderManager
    std::cerr << "Step 1: Creating OrderManager..." << std::endl;
    auto order_manager = std::make_shared<OrderManager>(100000.0, "test_trades.csv", "test_equity.csv");
    std::cerr << "OrderManager created." << std::endl;

    // Step 2: Create a DataReader for a known small test file
    std::cerr << "Step 2: Creating DataReader..." << std::endl;
    auto data_reader = std::make_unique<CSVDataReader>("data/raw_ticks_AAPL.csv");
    std::cerr << "DataReader created." << std::endl;

    // Step 3: Create the strategy
    std::cerr << "Step 3: Creating FillTrackingStrategy..." << std::endl;
    auto strategy = std::make_unique<FillTrackingStrategy>(order_manager);
    std::cerr << "FillTrackingStrategy created." << std::endl;

    // Step 4: Create the Backtester
    std::cerr << "Step 4: Creating Backtester..." << std::endl;
    Backtester backtester("AAPL", std::move(data_reader), std::move(strategy), order_manager, std::chrono::minutes(1));
    std::cerr << "Backtester created." << std::endl;

    // Step 5: Run the backtest
    std::cerr << "Step 5: Running backtest..." << std::endl;
    backtester.run();
    std::cerr << "Backtest completed." << std::endl;
    
    std::cerr << "=== MINIMAL TEST COMPLETED SUCCESSFULLY ===" << std::endl;
}

int main(int argc, char** argv) {
    run_minimal_test();
    return 0;
} 