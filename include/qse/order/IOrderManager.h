// In src/IOrderManager.h

#pragma once
#include "qse/data/Data.h"
#include <vector> // <-- Make sure <vector> is included
#include <string>
#include <map>

namespace qse {

    // An interface for an order manager that can execute trades and track positions
    // for multiple assets.
    class IOrderManager {
    public:
        virtual ~IOrderManager() = default;

        // Execute a buy order for a given symbol.
        virtual void execute_buy(const std::string& symbol, int quantity, double price) = 0;

        // Execute a sell order for a given symbol.
        virtual void execute_sell(const std::string& symbol, int quantity, double price) = 0;

        // Get the current position for a given symbol.
        virtual int get_position(const std::string& symbol) const = 0;

        // Get the total cash value of the portfolio.
        virtual double get_cash() const = 0;

        // A mechanism for the backtester to update the equity curve, providing all current market prices.
        virtual void record_equity(long long timestamp, const std::map<std::string, double>& market_prices) = 0;
    };

} // namespace qse