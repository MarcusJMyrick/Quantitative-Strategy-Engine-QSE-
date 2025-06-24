#pragma once

#include "qse/data/Data.h"
#include <unordered_map>
#include <string>

namespace qse {

/**
 * @brief Represents the top of the order book for a single symbol.
 */
struct TopOfBook {
    Price best_bid_price = 0.0;
    Volume best_bid_size = 0;
    Price best_ask_price = 0.0;
    Volume best_ask_size = 0;
    
    TopOfBook() = default;
    
    // Helper methods
    bool has_bid() const { return best_bid_size > 0; }
    bool has_ask() const { return best_ask_size > 0; }
    Price mid_price() const { return (best_bid_price + best_ask_price) / 2.0; }
    double spread() const { return best_ask_price - best_bid_price; }
};

/**
 * @brief Simulates an order book that maintains top-of-book for multiple symbols.
 * This is a simplified order book that only tracks the best bid/ask levels.
 */
class OrderBook {
public:
    OrderBook() = default;
    
    /**
     * @brief Updates the order book with a new tick.
     * @param tick The tick containing bid/ask information
     */
    void on_tick(const Tick& tick);
    
    /**
     * @brief Gets the current top of book for a symbol.
     * @param symbol The symbol to query
     * @return The top of book for the symbol, or empty TopOfBook if not found
     */
    const TopOfBook& top_of_book(const std::string& symbol) const;
    
    /**
     * @brief Consumes liquidity from the order book (for order fills).
     * @param symbol The symbol
     * @param side The side (BUY consumes ask, SELL consumes bid)
     * @param quantity The quantity to consume
     * @return The actual quantity consumed (may be less than requested)
     */
    Volume consume_liquidity(const std::string& symbol, Order::Side side, Volume quantity);

private:
    std::unordered_map<std::string, TopOfBook> books_;
};

} // namespace qse 