#include "Backtester.h"
#include "CSVDataReader.h"
#include "OrderManager.h"
#include "SMACrossoverStrategy.h"
#include <memory>

int main() {
    // Create components with realistic transaction costs
    auto data_reader = std::make_unique<qse::CSVDataReader>("data/SPY.parquet");
    auto order_manager = std::make_unique<qse::OrderManager>(
        100000.0,  // Starting cash
        1.0,       // Commission per trade ($1)
        0.01       // Slippage (1 cent per share)
    );
    auto strategy = std::make_unique<qse::SMACrossoverStrategy>(order_manager.get(), 10, 30);

    // Create and run the backtester
    qse::Backtester backtester(
        std::move(data_reader),
        std::move(strategy),
        std::unique_ptr<qse::IOrderManager>(std::move(order_manager))
    );
    backtester.run();

    return 0;
} 