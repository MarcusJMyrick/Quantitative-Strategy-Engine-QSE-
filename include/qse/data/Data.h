#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace qse {

// Use type aliases for clarity and easy changes later.
using Timestamp = std::chrono::system_clock::time_point;
using Price = double;
using Volume = uint64_t;
using OrderId = std::string;  // Order identifier

/**
 * @brief Represents a single price bar (OHLCV data).
 */
struct Bar {
    std::string symbol;    // Symbol/identifier for the asset
    Timestamp timestamp;   // Bar's start timestamp
    Price open;           // Opening price
    Price high;           // Highest price during the bar
    Price low;            // Lowest price during the bar
    Price close;          // Closing price
    Volume volume;        // Total volume traded during the bar

    Bar() = default; // Default constructor
};

/**
 * @brief Represents a single market trade event (a tick).
 */
struct Tick {
    std::string symbol;    // Symbol/identifier for the asset
    Timestamp timestamp;   // Tick's exact timestamp
    Price price;         // Price of the trade (last trade price)
    Price bid;           // Best bid price
    Price ask;           // Best ask price
    Volume bid_size;     // Best bid size
    Volume ask_size;     // Best ask size
    Volume volume;       // Volume of the trade
    
    Tick() = default; // Default constructor
    
    // Helper method to get mid price
    Price mid_price() const { return (bid + ask) / 2.0; }
};

/**
 * @brief Represents an order submitted to the market.
 * Enhanced for tick-level simulation with support for limit orders, IOC, and partial fills.
 */
struct Order {
    enum class Type { MARKET, LIMIT, IOC, TARGET_PERCENT };  // Added TARGET_PERCENT
    enum class Side { BUY, SELL };
    enum class Status { PENDING, PARTIALLY_FILLED, FILLED, CANCELLED, REJECTED };
    enum class TimeInForce { DAY, IOC, GTC };  // Day, Immediate-or-Cancel, Good-Till-Cancelled

    std::string order_id;
    std::string symbol;
    Type type;
    Side side;
    TimeInForce time_in_force;
    Price limit_price;        // For limit orders
    Volume quantity;          // Original order quantity
    Volume filled_quantity;   // How much has been filled
    Price avg_fill_price;     // Average fill price for partial fills
    Status status;
    Timestamp timestamp;      // When order was placed
    Timestamp expiry_time;    // For IOC orders
    double target_percent = 0.0; // Only used for TARGET_PERCENT orders
    
    // Helper methods
    bool is_active() const { return status == Status::PENDING || status == Status::PARTIALLY_FILLED; }
    bool is_filled() const { return status == Status::FILLED; }
    Volume remaining_quantity() const { return quantity - filled_quantity; }
    
    Order() = default;
};

// --- Data structures for logging trades ---

/**
 * @brief Represents the direction of a trade execution.
 */
enum class TradeType { BUY, SELL };

/**
 * @brief Represents a single executed trade with its associated costs.
 * This is used for generating the final performance and trade logs.
 */
struct Trade {
    Timestamp timestamp;
    
    // --- ADDITION: Added symbol to support multi-asset logging ---
    // This is crucial for distinguishing trades when running parallel backtests.
    std::string symbol;      

    TradeType type;
    Price price;
    int quantity;
    Price commission;
};

// --- NEW: Fill structure for order execution feedback ---
struct Fill {
    OrderId order_id;
    std::string symbol;
    Volume quantity;
    Price price;
    Timestamp timestamp;
    std::string side; // "BUY" or "SELL"
    
    Fill(OrderId id, const std::string& sym, Volume qty, Price px, Timestamp ts, const std::string& s)
        : order_id(id), symbol(sym), quantity(qty), price(px), timestamp(ts), side(s) {}
};

// --- NEW: Position structure for portfolio holdings ---
struct Position {
    std::string symbol;
    double quantity;  // Can be negative for short positions
    
    Position(const std::string& sym, double qty) : symbol(sym), quantity(qty) {}
    Position() = default;
};

// --- Timestamp helpers for ms conversions ---
inline Timestamp from_unix_ms(int64_t ms) {
    return Timestamp(std::chrono::milliseconds(ms));
}

inline int64_t to_unix_ms(const Timestamp& ts) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
}

} // namespace qse