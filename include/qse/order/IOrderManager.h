// In src/IOrderManager.h

#pragma once
#include "qse/data/Data.h"
#include <vector> // <-- Make sure <vector> is included
#include <string>
#include <map>
#include <optional>
#include <functional>

namespace qse {

    // An interface for an order manager that can execute trades and track positions
    // for multiple assets.
    class IOrderManager {
    public:
        virtual ~IOrderManager() = default;

        // --- NEW: Fill callback type ---
        using FillCallback = std::function<void(const Fill&)>;

        // --- Tick-level order management ---
        
        // Submit a market order
        virtual OrderId submit_market_order(const std::string& symbol, Order::Side side, Volume quantity) = 0;
        
        // Submit a limit order
        virtual OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity, 
                                         Price limit_price, Order::TimeInForce tif) = 0;
        
        // Cancel an existing order
        virtual bool cancel_order(const OrderId& order_id) = 0;
        
        // Process a tick and match orders
        virtual void process_tick(const Tick& tick) = 0;
        
        // --- NEW: Attempt fills against current order book ---
        // This should be called after order_book.on_tick() to match orders
        virtual void attempt_fills() = 0;
        
        // --- NEW: Set fill callback for strategy notifications ---
        virtual void set_fill_callback(FillCallback callback) = 0;
        
        // Get order information
        virtual std::optional<Order> get_order(const OrderId& order_id) const = 0;
        
        // Get all active orders for a symbol
        virtual std::vector<Order> get_active_orders(const std::string& symbol) const = 0;

        // --- Legacy methods (for backward compatibility) ---
        
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