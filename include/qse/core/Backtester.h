#pragma once

#include <memory>
#include <string> // <-- Add for std::string
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "qse/data/IDataReader.h"
#include "qse/strategy/IStrategy.h"
#include "qse/order/IOrderManager.h"
#include "qse/data/BarBuilder.h"
#include "qse/data/OrderBook.h"
#include "qse/core/BarRouter.h"

namespace qse {

class Backtester {
public:
    // --- CHANGE: The constructor now accepts the symbol ---
    Backtester(
        const std::string& symbol, // The symbol for this specific backtest (e.g., "SPY")
        std::unique_ptr<IDataReader> data_reader,
        std::unique_ptr<IStrategy> strategy,
        std::shared_ptr<IOrderManager> order_manager,
        const std::chrono::seconds& bar_interval = std::chrono::seconds(60) // Default to 1-minute bars
    );

    void run();

    void add_data_source(std::unique_ptr<IDataReader> data_reader);

private:
    void process_tick(const Tick& tick);

    std::string symbol_; // <-- Add member to store the symbol
    std::vector<std::unique_ptr<IDataReader>> data_readers_;
    std::unique_ptr<IStrategy> strategy_;
    std::shared_ptr<IOrderManager> order_manager_;
    
    // BarRouter dispatches bars to strategies interested in each symbol.
    BarRouter bar_router_;

    // One BarBuilder per symbol to avoid mixing OHLC between symbols.
    std::unordered_map<std::string, BarBuilder> bar_builders_;

    OrderBook order_book_;

    // Cache symbols that have already been registered with the router to
    // avoid duplicate registrations.
    std::unordered_set<std::string> registered_symbols_;

    std::chrono::seconds bar_interval_;
};

} // namespace qse