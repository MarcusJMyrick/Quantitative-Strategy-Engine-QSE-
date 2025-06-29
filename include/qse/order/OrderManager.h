#pragma once

#include "IOrderManager.h"
#include "qse/data/Data.h"
#include "qse/data/OrderBook.h"
#include "qse/core/Config.h"

#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

namespace qse {

    // A concrete implementation of the IOrderManager interface.
    // This class handles order execution, position tracking, and logging for multiple assets.
    class OrderManager : public IOrderManager {
    public:
        // Constructor with OrderBook integration
        OrderManager(const Config& config, OrderBook& order_book, const std::string& equity_curve_path, const std::string& tradelog_path);
        
        // Constructor now takes a Config object
        OrderManager(const Config& config, const std::string& equity_curve_path, const std::string& tradelog_path);
        
        // Legacy constructor for backward compatibility
        OrderManager(double initial_cash, const std::string& equity_curve_path, const std::string& tradelog_path);
        
        ~OrderManager() override;

        // Legacy methods
        void execute_buy(const std::string& symbol, int quantity, double price) override;
        void execute_sell(const std::string& symbol, int quantity, double price) override;
        int get_position(const std::string& symbol) const override;
        std::vector<Position> get_positions() const override;
        double get_cash() const override;
        void record_equity(long long timestamp, const std::map<std::string, double>& market_prices) override;

        // Tick-level order management methods
        OrderId submit_market_order(const std::string& symbol, Order::Side side, Volume quantity) override;
        OrderId submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity, 
                                  Price limit_price, Order::TimeInForce tif) override;
        bool cancel_order(const OrderId& order_id) override;
        void process_tick(const Tick& tick) override;
        
        // --- NEW: Attempt fills against current order book ---
        void attempt_fills() override;
        
        // --- NEW: Set fill callback for strategy notifications ---
        void set_fill_callback(FillCallback callback) override;
        
        std::optional<Order> get_order(const OrderId& order_id) const override;
        std::vector<Order> get_active_orders(const std::string& symbol) const override;

    private:
        // Configuration for slippage coefficients
        const Config* config_;
        
        // Order book reference
        OrderBook* order_book_;
        
        // Portfolio state
        double cash_;
        std::map<std::string, int> positions_;
        
        // Order book
        std::unordered_map<OrderId, Order> orders_;
        std::unordered_map<std::string, std::vector<OrderId>> symbol_orders_;
        
        // Order ID generation
        int next_order_id_;
        
        // File outputs
        std::ofstream equity_curve_file_;
        std::ofstream tradelog_file_;
        
        // --- NEW: Fill callback for strategy notifications ---
        FillCallback fill_callback_;
        
        // Helper methods
        OrderId generate_order_id();
        void add_order_to_book(const Order& order);
        void remove_order_from_book(const OrderId& order_id);
        void match_orders_for_symbol(const std::string& symbol, const Tick& tick);
        void fill_order(Order& order, Volume fill_qty, Price fill_price, const Tick& tick);
        void cancel_ioc_orders(const std::string& symbol, const Tick& tick);
        
        // New helper methods for OrderBook integration
        Volume process_limit_order_fill(Order& order, const TopOfBook& tob);
        Volume process_ioc_order_fill(Order& order, const TopOfBook& tob);
        
        // Legacy helper methods
        void log_trade(long long timestamp, const std::string& symbol, const std::string& type, int quantity, double price);
        double calculate_holdings_value(const std::map<std::string, double>& market_prices) const;
    };

} // namespace qse