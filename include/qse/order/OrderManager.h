#pragma once

#include "IOrderManager.h"
#include "qse/data/Data.h"

#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <memory>
#include <optional>

namespace qse {

    // A concrete implementation of the IOrderManager interface.
    // This class handles order execution, position tracking, and logging for multiple assets.
    class OrderManager : public IOrderManager {
    public:
        // Constructor initializes the order manager with starting capital and file paths for logging.
        OrderManager(double initial_cash, const std::string& equity_curve_path, const std::string& tradelog_path);
        ~OrderManager() override;

        // Execute a buy order for a given symbol.
        void execute_buy(const std::string& symbol, int quantity, double price) override;

        // Execute a sell order for a given symbol.
        void execute_sell(const std::string& symbol, int quantity, double price) override;

        // Get the current position for a given symbol.
        int get_position(const std::string& symbol) const override;
        
        // Get the total cash value of the portfolio.
        double get_cash() const override;

        // --- Tick-level order management ---
        
        // Submit a market order
        OrderId submit_market_order(const std::string& symbol, Order::Side side, Volume quantity) override;
        
        // Submit a limit order
        OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity, 
                                 Price limit_price, Order::TimeInForce tif) override;
        
        // Cancel an existing order
        bool cancel_order(const OrderId& order_id) override;
        
        // Process a tick and match orders
        void process_tick(const Tick& tick) override;
        
        // Get order information
        std::optional<Order> get_order(const OrderId& order_id) const override;
        
        // Get all active orders for a symbol
        std::vector<Order> get_active_orders(const std::string& symbol) const override;

        // Update the equity curve with the current portfolio value at a given timestamp.
        void record_equity(long long timestamp, const std::map<std::string, double>& market_prices) override;

    private:
        // Log a trade to the tradelog file.
        void log_trade(long long timestamp, const std::string& symbol, const std::string& type, int quantity, double price);

        // Calculate the total value of all holdings across all symbols.
        double calculate_holdings_value(const std::map<std::string, double>& market_prices) const;

        double cash_;                           // Current cash balance.
        std::map<std::string, int> positions_;  // Map of symbol to position quantity.
        std::ofstream equity_curve_file_;       // File stream for the equity curve log.
        std::ofstream tradelog_file_;           // File stream for the trade log.
    };

} // namespace qse