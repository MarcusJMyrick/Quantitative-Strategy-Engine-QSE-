#include "Backtester.h"
#include <iostream>

namespace qse {

Backtester::Backtester(
    std::unique_ptr<IDataReader> data_reader,
    std::unique_ptr<IStrategy> strategy,
    std::unique_ptr<OrderManager> order_manager)
    : data_reader_(std::move(data_reader)),
      strategy_(std::move(strategy)),
      order_manager_(std::move(order_manager)) {}

void Backtester::run() {
    std::cout << "--- Starting Backtest ---" << std::endl;

    // 1. Get all market data from the reader.
    auto bars = data_reader_->read_all_bars();

    // 2. Loop through each bar of data.
    for (const auto& bar : bars) {
        // 3. Pass the current bar to the strategy.
        strategy_->on_bar(bar);
    }

    std::cout << "--- Backtest Finished ---" << std::endl;
    std::cout << "Final Position: " << order_manager_->get_position() << std::endl;
}

} // namespace qse 