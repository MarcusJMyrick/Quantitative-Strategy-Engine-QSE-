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
    Timestamp timestamp;  // Tick's exact timestamp
    Price price;         // Price of the trade
    Volume volume;       // Volume of the trade
    
    // Note: Symbol can be added if the data source provides it per-tick.
    // For now, we assume the DataReader handles one symbol at a time.
    
    Tick() = default; // Default constructor
};

/**
 * @brief Represents an order submitted to the market.
 * This is a foundational struct for a future, more advanced OrderManager.
 */
struct Order {
    enum class Type { MARKET, LIMIT, STOP, STOP_LIMIT };
    enum class Side { BUY, SELL };
    enum class Status { PENDING, FILLED, CANCELLED, REJECTED };

    std::string order_id;
    std::string symbol;
    Type type;
    Side side;
    Price price;
    Volume quantity;
    Status status;
    Timestamp timestamp;
    
    Order() = default; // Default constructor
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

} // namespace qse