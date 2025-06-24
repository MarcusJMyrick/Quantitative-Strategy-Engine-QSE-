#include "qse/order/OrderManager.h"
#include "qse/data/Data.h" // Required for the Trade struct

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <algorithm>

namespace qse {

OrderManager::OrderManager(double initial_cash, const std::string& equity_curve_path, const std::string& tradelog_path)
    : cash_(initial_cash), next_order_id_(1) {

    equity_curve_file_.open(equity_curve_path);
    if (!equity_curve_file_.is_open()) {
        throw std::runtime_error("Could not open equity curve file: " + equity_curve_path);
    }
    equity_curve_file_ << "timestamp,equity\n"; // Write header

    tradelog_file_.open(tradelog_path);
    if (!tradelog_file_.is_open()) {
        throw std::runtime_error("Could not open tradelog file: " + tradelog_path);
    }
    tradelog_file_ << "timestamp,symbol,type,quantity,price,cash\n"; // Write header
}

OrderManager::~OrderManager() {
    if (equity_curve_file_.is_open()) {
        equity_curve_file_.close();
    }
    if (tradelog_file_.is_open()) {
        tradelog_file_.close();
    }
}

void OrderManager::execute_buy(const std::string& symbol, int quantity, double price) {
    // Add robustness: ignore invalid order quantities.
    if (quantity <= 0) {
        std::cerr << "WARNING: Attempted to buy non-positive quantity: " << quantity << std::endl;
        return;
    }

    double cost = quantity * price;
    if (cost > cash_) {
        // Not enough cash to execute the trade, log it and return.
        // In a real system, this might be handled differently (e.g., partial fill, error).
        std::cerr << "WARNING: Not enough cash to execute buy order for " << quantity 
                  << " of " << symbol << " at price " << price << ". Have " << cash_ 
                  << ", need " << cost << std::endl;
        return;
    }
    cash_ -= cost;
    positions_[symbol] += quantity; // Update position for the specific symbol
    log_trade(0, symbol, "BUY", quantity, price); // Timestamp will be updated by backtester
}

void OrderManager::execute_sell(const std::string& symbol, int quantity, double price) {
    // Add robustness: ignore invalid order quantities.
    if (quantity <= 0) {
        std::cerr << "WARNING: Attempted to sell non-positive quantity: " << quantity << std::endl;
        return;
    }
    
    // For short selling, we don't check the current position, but in a real system
    // you would need to manage margin requirements. For now, we assume we can always short.
    cash_ += quantity * price;
    positions_[symbol] -= quantity; // Update position for the specific symbol
    log_trade(0, symbol, "SELL", quantity, price); // Timestamp will be updated by backtester
}

int OrderManager::get_position(const std::string& symbol) const {
    // If the symbol is not in our map, we have no position.
    if (positions_.find(symbol) == positions_.end()) {
        return 0;
    }
    // Otherwise, return the stored position.
    return positions_.at(symbol);
}

double OrderManager::get_cash() const {
    return cash_;
}

void OrderManager::record_equity(long long timestamp, const std::map<std::string, double>& market_prices) {
    if (equity_curve_file_.is_open()) {
        double holdings_value = calculate_holdings_value(market_prices);
        double total_equity = cash_ + holdings_value;
        equity_curve_file_ << timestamp << "," << total_equity << "\n";
    }
}

void OrderManager::log_trade(long long timestamp, const std::string& symbol, const std::string& type, int quantity, double price) {
    if (tradelog_file_.is_open()) {
        // We'll update the timestamp later. For now, just log the core trade details.
        // A more sophisticated logger might buffer trades and associate them with timestamps.
        tradelog_file_ << timestamp << "," << symbol << "," << type << "," << quantity << "," << price << "," << cash_ << "\n";
    }
}

double OrderManager::calculate_holdings_value(const std::map<std::string, double>& market_prices) const {
    double total_value = 0.0;
    for(const auto& pair : positions_) {
        const std::string& symbol = pair.first;
        int quantity = pair.second;
        
        if(market_prices.count(symbol)) {
            total_value += quantity * market_prices.at(symbol);
        }
    }
    return total_value;
}

// --- Tick-level order management stub implementations ---

OrderId OrderManager::submit_market_order(const std::string& symbol, Order::Side side, Volume quantity) {
    OrderId order_id = generate_order_id();
    
    Order order;
    order.order_id = order_id;
    order.symbol = symbol;
    order.type = Order::Type::MARKET;
    order.side = side;
    order.time_in_force = Order::TimeInForce::DAY;
    order.quantity = quantity;
    order.filled_quantity = 0;
    order.avg_fill_price = 0.0;
    order.status = Order::Status::PENDING;
    order.timestamp = std::chrono::system_clock::now();
    
    add_order_to_book(order);
    return order_id;
}

OrderId OrderManager::submit_limit_order(const std::string& symbol, Order::Side side, Volume quantity, 
                                        Price limit_price, Order::TimeInForce tif) {
    OrderId order_id = generate_order_id();
    
    Order order;
    order.order_id = order_id;
    order.symbol = symbol;
    order.type = Order::Type::LIMIT;
    order.side = side;
    order.time_in_force = tif;
    order.limit_price = limit_price;
    order.quantity = quantity;
    order.filled_quantity = 0;
    order.avg_fill_price = 0.0;
    order.status = Order::Status::PENDING;
    order.timestamp = std::chrono::system_clock::now();
    
    // For IOC orders, set expiry time to current time
    if (tif == Order::TimeInForce::IOC) {
        order.expiry_time = order.timestamp;
    }
    
    add_order_to_book(order);
    return order_id;
}

bool OrderManager::cancel_order(const OrderId& order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false; // Order not found
    }
    
    Order& order = it->second;
    if (order.status != Order::Status::PENDING && order.status != Order::Status::PARTIALLY_FILLED) {
        return false; // Order cannot be cancelled
    }
    
    order.status = Order::Status::CANCELLED;
    remove_order_from_book(order_id);
    return true;
}

void OrderManager::process_tick(const Tick& tick) {
    // Use the tick's symbol for order matching
    std::string symbol = tick.symbol;
    
    std::cout << "DEBUG: Processing tick for " << symbol << " bid=" << tick.bid << " ask=" << tick.ask << " mid=" << tick.mid_price() << std::endl;
    
    // Match orders for this symbol first
    match_orders_for_symbol(symbol, tick);
    
    // Cancel IOC orders that weren't filled immediately
    cancel_ioc_orders(symbol, tick);
}

std::optional<Order> OrderManager::get_order(const OrderId& order_id) const {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Order> OrderManager::get_active_orders(const std::string& symbol) const {
    std::vector<Order> active_orders;
    
    auto symbol_it = symbol_orders_.find(symbol);
    if (symbol_it == symbol_orders_.end()) {
        return active_orders;
    }
    
    for (const OrderId& order_id : symbol_it->second) {
        auto order_it = orders_.find(order_id);
        if (order_it != orders_.end()) {
            const Order& order = order_it->second;
            if (order.is_active()) {
                active_orders.push_back(order);
            }
        }
    }
    
    return active_orders;
}

// --- Helper method implementations ---

OrderId OrderManager::generate_order_id() {
    return "ORDER_" + std::to_string(next_order_id_++);
}

void OrderManager::add_order_to_book(const Order& order) {
    orders_[order.order_id] = order;
    symbol_orders_[order.symbol].push_back(order.order_id);
}

void OrderManager::remove_order_from_book(const OrderId& order_id) {
    auto order_it = orders_.find(order_id);
    if (order_it == orders_.end()) {
        return;
    }
    
    const std::string& symbol = order_it->second.symbol;
    
    // Remove from symbol_orders_
    auto symbol_it = symbol_orders_.find(symbol);
    if (symbol_it != symbol_orders_.end()) {
        auto& order_ids = symbol_it->second;
        order_ids.erase(std::remove(order_ids.begin(), order_ids.end(), order_id), order_ids.end());
        
        // Remove empty symbol entries
        if (order_ids.empty()) {
            symbol_orders_.erase(symbol_it);
        }
    }
}

void OrderManager::match_orders_for_symbol(const std::string& symbol, const Tick& tick) {
    auto symbol_it = symbol_orders_.find(symbol);
    if (symbol_it == symbol_orders_.end()) {
        std::cout << "DEBUG: No orders found for symbol " << symbol << std::endl;
        return;
    }
    
    std::vector<OrderId>& order_ids = symbol_it->second;
    std::cout << "DEBUG: Found " << order_ids.size() << " orders for symbol " << symbol << std::endl;
    std::vector<OrderId> to_remove;
    
    for (const OrderId& order_id : order_ids) {
        auto order_it = orders_.find(order_id);
        if (order_it == orders_.end()) {
            continue;
        }
        
        Order& order = order_it->second;
        if (!order.is_active()) {
            std::cout << "DEBUG: Order " << order_id << " is not active, status=" << static_cast<int>(order.status) << std::endl;
            continue;
        }
        
        std::cout << "DEBUG: Processing order " << order_id << " type=" << static_cast<int>(order.type) << " side=" << static_cast<int>(order.side) << " qty=" << order.quantity << std::endl;
        
        // Check if order should be filled
        bool should_fill = false;
        Price fill_price = 0.0;
        
        if (order.type == Order::Type::MARKET) {
            // Market orders fill immediately at mid price
            should_fill = true;
            fill_price = tick.mid_price();
            std::cout << "DEBUG: Market order should fill at " << fill_price << std::endl;
        } else if (order.type == Order::Type::LIMIT) {
            // Limit orders fill when price crosses the limit
            if (order.side == Order::Side::BUY && tick.ask <= order.limit_price) {
                should_fill = true;
                fill_price = order.limit_price;
                std::cout << "DEBUG: Limit buy order should fill at " << fill_price << std::endl;
            } else if (order.side == Order::Side::SELL && tick.bid >= order.limit_price) {
                should_fill = true;
                fill_price = order.limit_price;
                std::cout << "DEBUG: Limit sell order should fill at " << fill_price << std::endl;
            } else {
                std::cout << "DEBUG: Limit order not filled - buy side=" << (order.side == Order::Side::BUY) << " ask=" << tick.ask << " limit=" << order.limit_price << std::endl;
            }
        }
        
        if (should_fill) {
            Volume remaining_qty = order.remaining_quantity();
            Volume fill_qty = std::min(remaining_qty, tick.volume);
            
            std::cout << "DEBUG: Filling order with qty=" << fill_qty << " at price=" << fill_price << std::endl;
            fill_order(order, fill_qty, fill_price, tick);
            
            if (order.is_filled()) {
                to_remove.push_back(order_id);
            }
        }
    }
    
    // Remove filled orders from symbol_orders_
    for (const OrderId& order_id : to_remove) {
        remove_order_from_book(order_id);
    }
}

void OrderManager::fill_order(Order& order, Volume fill_qty, Price fill_price, const Tick& tick) {
    // Update order
    Volume old_filled = order.filled_quantity;
    order.filled_quantity += fill_qty;
    
    // Update average fill price
    double total_cost = order.avg_fill_price * old_filled + fill_price * fill_qty;
    order.avg_fill_price = total_cost / order.filled_quantity;
    
    // Update status
    if (order.filled_quantity >= order.quantity) {
        order.status = Order::Status::FILLED;
    } else {
        order.status = Order::Status::PARTIALLY_FILLED;
    }
    
    // Update portfolio
    if (order.side == Order::Side::BUY) {
        double cost = fill_qty * fill_price;
        if (cost <= cash_) {
            cash_ -= cost;
            positions_[order.symbol] += fill_qty;
            log_trade(to_unix_ms(tick.timestamp), order.symbol, "BUY", fill_qty, fill_price);
        }
    } else { // SELL
        double proceeds = fill_qty * fill_price;
        cash_ += proceeds;
        positions_[order.symbol] -= fill_qty;
        log_trade(to_unix_ms(tick.timestamp), order.symbol, "SELL", fill_qty, fill_price);
    }
}

void OrderManager::cancel_ioc_orders(const std::string& symbol, const Tick& tick) {
    auto symbol_it = symbol_orders_.find(symbol);
    if (symbol_it == symbol_orders_.end()) {
        return;
    }
    
    std::vector<OrderId>& order_ids = symbol_it->second;
    std::vector<OrderId> to_cancel;
    
    for (const OrderId& order_id : order_ids) {
        auto order_it = orders_.find(order_id);
        if (order_it == orders_.end()) {
            continue;
        }
        
        Order& order = order_it->second;
        if (order.time_in_force == Order::TimeInForce::IOC && order.filled_quantity == 0) {
            to_cancel.push_back(order_id);
        }
    }
    
    for (const OrderId& order_id : to_cancel) {
        auto order_it = orders_.find(order_id);
        if (order_it != orders_.end()) {
            order_it->second.status = Order::Status::CANCELLED;
        }
        remove_order_from_book(order_id);
    }
}

} // namespace qse