#include "qse/order/OrderManager.h"
#include "qse/data/Data.h" // Required for the Trade struct

#include <iostream>
#include <stdexcept>

namespace qse {

OrderManager::OrderManager(double initial_cash, const std::string& equity_curve_path, const std::string& tradelog_path)
    : cash_(initial_cash) {

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

} // namespace qse