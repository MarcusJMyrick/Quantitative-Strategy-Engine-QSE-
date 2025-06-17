#pragma once

#include <memory>
#include <string> // <-- Add for std::string

#include "IDataReader.h"
#include "IStrategy.h"
#include "IOrderManager.h"

namespace qse {

class Backtester {
public:
    // --- CHANGE: The constructor now accepts the symbol ---
    Backtester(
        const std::string& symbol, // The symbol for this specific backtest (e.g., "SPY")
        std::unique_ptr<IDataReader> data_reader,
        std::unique_ptr<IStrategy> strategy,
        std::unique_ptr<IOrderManager> order_manager
    );

    void run();

private:
    std::string symbol_; // <-- Add member to store the symbol
    std::unique_ptr<IDataReader> data_reader_;
    std::unique_ptr<IStrategy> strategy_;
    std::unique_ptr<IOrderManager> order_manager_;
};

} // namespace qse