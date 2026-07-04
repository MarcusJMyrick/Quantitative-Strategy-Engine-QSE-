#pragma once

#include "qse/data/Data.h"
#include <map>
#include <deque>
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace qse {

// Queue identifier for FIFO ordering
using QueueId = std::uint64_t;

/**
 * @brief Represents a single price level in the order book.
 * Contains the total size at this price and a FIFO queue of order IDs.
 */
struct Level {
    Volume total_size = 0;
    std::deque<OrderId> queue;
    Price price = 0.0;  // Metadata field for convenience
    
    // Position tracking for O(1) queue position lookups
    std::unordered_map<OrderId, size_t> position_map;

    // Remaining size of each resting order (partially filled orders shrink here)
    std::unordered_map<OrderId, Volume> order_sizes;
    
    Level() = default;
    Level(Price p) : price(p) {}
    
    // Helper methods
    bool empty() const { return queue.empty() && total_size == 0; }
    size_t queue_size() const { return queue.size(); }
};

/**
 * @brief Represents the top of the order book for a single symbol.
 * Kept for backward compatibility with existing code.
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
 * @brief Full-depth order book that maintains price levels with FIFO queues.
 * This replaces the simplified top-of-book only implementation.
 */
class OrderBookFullDepth {
public:
    OrderBookFullDepth() = default;
    
    // --- Queue ID Management ---
    
    /**
     * @brief Allocates a unique queue ID for FIFO ordering.
     * @return A unique queue identifier
     */
    QueueId allocate_queue_id();
    
    /**
     * @brief Resets the queue ID counter (for testing only).
     */
    static void reset_queue_id_counter();
    
    // --- Level Management ---
    
    /**
     * @brief Adds a new price level if it doesn't exist.
     * @param side The side (BUY/SELL)
     * @param price The price level
     */
    void add_level(Order::Side side, Price price);
    
    /**
     * @brief Removes a price level if it's empty.
     * @param side The side (BUY/SELL)
     * @param price The price level
     */
    void remove_level_if_empty(Order::Side side, Price price);
    
    /**
     * @brief Checks if a price level exists.
     * @param side The side (BUY/SELL)
     * @param price The price level
     * @return True if the level exists
     */
    bool has_level(Order::Side side, Price price) const;
    
    // --- Order Queue Management ---
    
    /**
     * @brief Adds an order to the back of a price level's queue.
     * @param side The side (BUY/SELL)
     * @param price The price level
     * @param order_id The order ID
     * @param size The order size
     * @return The queue ID assigned to this order
     */
    QueueId enqueue_order(Order::Side side, Price price, OrderId order_id, Volume size);
    
    /**
     * @brief Removes and returns the order ID at the front of a price level's queue.
     * @param side The side (BUY/SELL)
     * @param price The price level
     * @return The order ID, or empty string if queue is empty
     */
    OrderId dequeue_head(Order::Side side, Price price);
    
    /**
     * @brief Cancels an order by QueueId and updates positions of remaining orders.
     * @param queue_id The queue ID of the order to cancel
     * @return True if the order was found and cancelled, false otherwise
     */
    bool cancel_order(QueueId queue_id);
    
    /**
     * @brief Gets the queue position of an order (1-based).
     * @param side The side (BUY/SELL)
     * @param price The price level
     * @param order_id The order ID
     * @return The position (1 = head, 0 = not found)
     */
    size_t queue_position(Order::Side side, Price price, OrderId order_id) const;
    
    /**
     * @brief Gets the queue position of an order by QueueId (1-based).
     * Searches both bids and asks for the given QueueId.
     * @param queue_id The queue ID to look up
     * @return The position (1 = head, 0 = not found)
     */
    size_t queue_position(QueueId queue_id) const;
    
    // --- Price Level Queries ---
    
    /**
     * @brief Gets the top N prices for a side.
     * @param side The side (BUY/SELL)
     * @param n The number of prices to return
     * @return Vector of prices (best first)
     */
    std::vector<Price> top_n_prices(Order::Side side, size_t n) const;
    
    /**
     * @brief Gets the best bid and ask prices.
     * @return TopOfBook structure
     */
    TopOfBook top_of_book() const;
    
    // --- Legacy Interface (for backward compatibility) ---
    
    /**
     * @brief Updates the order book with a new tick.
     * @param tick The tick containing bid/ask information
     */
    void on_tick(const Tick& tick);
    
    /**
     * @brief Consumes liquidity from the order book (for order fills).
     * @param side The side (BUY consumes ask, SELL consumes bid)
     * @param quantity The quantity to consume (non-positive consumes nothing)
     * @return The actual quantity consumed (may be less than requested)
     */
    Volume consume_liquidity(Order::Side side, std::int64_t quantity);

    /**
     * @brief Executes a market order by walking the book and consuming liquidity.
     * @param side The side of the market order (BUY consumes asks, SELL consumes bids)
     * @param quantity The quantity to fill (non-positive fills nothing)
     * @return A pair containing (filled_quantity, average_fill_price)
     */
    std::pair<Volume, Price> fill_market(Order::Side side, std::int64_t quantity);

private:
    // Maps price -> Level, with proper ordering
    // Bids: descending order (best bid first)
    // Asks: ascending order (best ask first)
    std::map<Price, Level, std::greater<Price>> bids_;  // greater for descending
    std::map<Price, Level, std::less<Price>> asks_;     // less for ascending
    
    // Global queue ID generator for FIFO ordering
    static std::atomic<QueueId> next_queue_id_;
    
    // Mapping from QueueId to OrderId for position lookups
    std::unordered_map<QueueId, OrderId> queue_id_to_order_id_;
    
    // Helper methods
    const std::map<Price, Level, std::greater<Price>>& get_bids() const { return bids_; }
    const std::map<Price, Level, std::less<Price>>& get_asks() const { return asks_; }
    std::map<Price, Level, std::greater<Price>>& get_bids() { return bids_; }
    std::map<Price, Level, std::less<Price>>& get_asks() { return asks_; }
};

} // namespace qse 