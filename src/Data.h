#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace qse {

// Timestamp type using system_clock
using Timestamp = std::chrono::system_clock::time_point;

// Price type using double for precision
using Price = double;

// Volume type using uint64_t for large numbers
using Volume = uint64_t;

/**
 * @brief Represents a single price bar (OHLCV data)
 */
struct Bar {
    Timestamp timestamp;  // Bar's timestamp
    Price open;          // Opening price
    Price high;          // Highest price
    Price low;           // Lowest price
    Price close;         // Closing price
    Volume volume;       // Trading volume

    // Default constructor
    Bar() = default;

    // Constructor with all fields
    Bar(Timestamp ts, Price o, Price h, Price l, Price c, Volume vol)
        : timestamp(ts), open(o), high(h), low(l), close(c), volume(vol) {}
};

/**
 * @brief Represents a single market tick
 */
struct Tick {
    Timestamp timestamp;  // Tick's timestamp
    Price price;         // Last traded price
    Volume volume;       // Volume at this price
    std::string symbol;  // Trading symbol

    // Default constructor
    Tick() = default;

    // Constructor with all fields
    Tick(Timestamp ts, Price p, Volume vol, const std::string& sym)
        : timestamp(ts), price(p), volume(vol), symbol(sym) {}
};

/**
 * @brief Represents an order in the system
 */
struct Order {
    enum class Type {
        MARKET,
        LIMIT,
        STOP,
        STOP_LIMIT
    };

    enum class Side {
        BUY,
        SELL
    };

    enum class Status {
        PENDING,
        FILLED,
        CANCELLED,
        REJECTED
    };

    std::string order_id;     // Unique order identifier
    std::string symbol;       // Trading symbol
    Type type;               // Order type
    Side side;               // Order side
    Price price;             // Order price
    Volume quantity;         // Order quantity
    Status status;           // Order status
    Timestamp timestamp;     // Order timestamp

    // Default constructor
    Order() = default;

    // Constructor with all fields
    Order(const std::string& id, const std::string& sym, Type t, Side s,
          Price p, Volume q, Status st, Timestamp ts)
        : order_id(id), symbol(sym), type(t), side(s), price(p),
          quantity(q), status(st), timestamp(ts) {}
};

} // namespace qse 