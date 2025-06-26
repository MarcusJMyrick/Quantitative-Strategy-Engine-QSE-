#pragma once

#include "qse/strategy/IStrategy.h"
#include "qse/order/IOrderManager.h"
#include "qse/data/Data.h"
#include "qse/strategy/MovingAverage.h"

namespace qse {

/**
 * @brief A simple moving average crossover strategy that operates on bars.
 * 
 * This strategy builds two moving averages (short and long) on bar close prices
 * and generates buy/sell signals when the short MA crosses above/below the long MA.
 * It only processes bars for its configured symbol.
 */
class SMACrossoverStrategy : public qse::IStrategy {
public:
    /**
     * @brief Constructor for the SMA Crossover Strategy.
     * @param order_manager Pointer to the order manager for executing trades.
     * @param short_window The window size for the short moving average.
     * @param long_window The window size for the long moving average.
     * @param symbol The symbol to trade (e.g., "SPY").
     */
    SMACrossoverStrategy(IOrderManager* order_manager, size_t short_window, size_t long_window, const std::string& symbol);

    // Process an incoming tick (ignores all ticks)
    void on_tick(const qse::Tick& tick) override;

    // Process an incoming bar - this is the main logic for the strategy
    void on_bar(const qse::Bar& bar) override;

private:
    IOrderManager* order_manager_;
    MovingAverage short_ma_;
    MovingAverage long_ma_;
    std::string symbol_;
};

} // namespace qse