#include "qse/data/OrderBookFullDepth.h"
#include <cstdint>
#include <iostream>
#include <algorithm>

namespace qse {

// Static member definition
std::atomic<QueueId> OrderBookFullDepth::next_queue_id_{1};

// --- Queue ID Management ---

QueueId OrderBookFullDepth::allocate_queue_id() {
    return next_queue_id_.fetch_add(1) + 1;
}

void OrderBookFullDepth::reset_queue_id_counter() {
    next_queue_id_.store(0);
}

// --- Level Management ---

void OrderBookFullDepth::add_level(Order::Side side, Price price) {
    if (side == Order::Side::BUY) {
        auto& bids = get_bids();
        if (bids.find(price) == bids.end()) {
            bids[price] = Level(price);
        }
    } else { // SELL
        auto& asks = get_asks();
        if (asks.find(price) == asks.end()) {
            asks[price] = Level(price);
        }
    }
}

void OrderBookFullDepth::remove_level_if_empty(Order::Side side, Price price) {
    if (side == Order::Side::BUY) {
        auto& bids = get_bids();
        auto it = bids.find(price);
        if (it != bids.end() && it->second.empty()) {
            bids.erase(it);
        }
    } else { // SELL
        auto& asks = get_asks();
        auto it = asks.find(price);
        if (it != asks.end() && it->second.empty()) {
            asks.erase(it);
        }
    }
}

bool OrderBookFullDepth::has_level(Order::Side side, Price price) const {
    if (side == Order::Side::BUY) {
        return get_bids().find(price) != get_bids().end();
    } else { // SELL
        return get_asks().find(price) != get_asks().end();
    }
}

// --- Order Queue Management ---

QueueId OrderBookFullDepth::enqueue_order(Order::Side side, Price price, OrderId order_id, Volume size) {
    // Ensure the price level exists
    add_level(side, price);

    // Obtain reference to the level
    Level* level_ptr;
    if (side == Order::Side::BUY) {
        level_ptr = &get_bids()[price];
    } else {
        level_ptr = &get_asks()[price];
    }
    Level& level = *level_ptr;

    // Reject duplicate OrderId at the same price level
    if (level.position_map.find(order_id) != level.position_map.end()) {
        return 0; // Duplicate detected – do not enqueue
    }

    // Allocate a unique QueueId and persist mapping
    QueueId queue_id = allocate_queue_id();
    queue_id_to_order_id_[queue_id] = order_id;

    // Enqueue the order and update position tracking
    level.queue.push_back(order_id);
    level.total_size += size;
    level.order_sizes[order_id] = size;
    level.position_map[order_id] = level.queue.size(); // 1-based position

    return queue_id;
}

QueueId OrderBookFullDepth::enqueue_order_front(Order::Side side, Price price, OrderId order_id, Volume size) {
    add_level(side, price);

    Level& level = (side == Order::Side::BUY) ? get_bids()[price] : get_asks()[price];

    // Reject duplicate OrderId at the same price level
    if (level.position_map.find(order_id) != level.position_map.end()) {
        return 0;
    }

    QueueId queue_id = allocate_queue_id();
    queue_id_to_order_id_[queue_id] = order_id;

    // Everyone already queued moves one position back
    for (auto& pos_pair : level.position_map) {
        pos_pair.second++;
    }
    level.queue.push_front(order_id);
    level.total_size += size;
    level.order_sizes[order_id] = size;
    level.position_map[order_id] = 1;

    return queue_id;
}

std::vector<std::pair<OrderId, Volume>> OrderBookFullDepth::consume_at_price(Order::Side side, Price price, Volume quantity) {
    std::vector<std::pair<OrderId, Volume>> consumed;

    auto process = [&](auto& levels) {
        auto it = levels.find(price);
        if (it == levels.end()) {
            return;
        }
        Level& level = it->second;
        Volume remaining = quantity;

        while (!level.queue.empty() && remaining > 0) {
            const OrderId& head_id = level.queue.front();
            auto size_it = level.order_sizes.find(head_id);
            Volume head_size = (size_it != level.order_sizes.end()) ? size_it->second : 0;

            if (head_size <= remaining) {
                // Fully consume the head order (skip zero-size stragglers)
                if (head_size > 0) {
                    consumed.emplace_back(head_id, head_size);
                }
                remaining -= head_size;
                dequeue_head(side, price);
            } else {
                // Partially consume the head order; it keeps its queue position
                consumed.emplace_back(head_id, remaining);
                size_it->second -= remaining;
                level.total_size -= remaining;
                remaining = 0;
            }
        }

        if (level.queue.empty()) {
            levels.erase(it);
        }
    };

    if (side == Order::Side::BUY) {
        process(get_bids());
    } else {
        process(get_asks());
    }

    return consumed;
}

OrderId OrderBookFullDepth::dequeue_head(Order::Side side, Price price) {
    if (side == Order::Side::BUY) {
        auto& bids = get_bids();
        auto it = bids.find(price);
        if (it != bids.end() && !it->second.queue.empty()) {
            OrderId order_id = it->second.queue.front();
            it->second.queue.pop_front();

            // Release the order's size from the level totals
            auto size_it = it->second.order_sizes.find(order_id);
            if (size_it != it->second.order_sizes.end()) {
                it->second.total_size -= size_it->second;
                it->second.order_sizes.erase(size_it);
            }

            // Remove from position map and update positions of remaining orders
            it->second.position_map.erase(order_id);
            for (auto& pos_pair : it->second.position_map) {
                pos_pair.second--; // Decrement all positions since head was removed
            }
            
            // Remove from queue_id_to_order_id_ mapping
            for (auto map_it = queue_id_to_order_id_.begin(); map_it != queue_id_to_order_id_.end(); ++map_it) {
                if (map_it->second == order_id) {
                    queue_id_to_order_id_.erase(map_it);
                    break;
                }
            }
            
            return order_id;
        }
    } else { // SELL
        auto& asks = get_asks();
        auto it = asks.find(price);
        if (it != asks.end() && !it->second.queue.empty()) {
            OrderId order_id = it->second.queue.front();
            it->second.queue.pop_front();

            // Release the order's size from the level totals
            auto size_it = it->second.order_sizes.find(order_id);
            if (size_it != it->second.order_sizes.end()) {
                it->second.total_size -= size_it->second;
                it->second.order_sizes.erase(size_it);
            }

            // Remove from position map and update positions of remaining orders
            it->second.position_map.erase(order_id);
            for (auto& pos_pair : it->second.position_map) {
                pos_pair.second--; // Decrement all positions since head was removed
            }
            
            // Remove from queue_id_to_order_id_ mapping
            for (auto map_it = queue_id_to_order_id_.begin(); map_it != queue_id_to_order_id_.end(); ++map_it) {
                if (map_it->second == order_id) {
                    queue_id_to_order_id_.erase(map_it);
                    break;
                }
            }
            
            return order_id;
        }
    }
    return ""; // Empty string if not found
}

size_t OrderBookFullDepth::queue_position(Order::Side side, Price price, OrderId order_id) const {
    if (side == Order::Side::BUY) {
        const auto& bids = get_bids();
        auto it = bids.find(price);
        if (it != bids.end()) {
            auto pos_it = it->second.position_map.find(order_id);
            if (pos_it != it->second.position_map.end()) {
                return pos_it->second;
            }
        }
    } else { // SELL
        const auto& asks = get_asks();
        auto it = asks.find(price);
        if (it != asks.end()) {
            auto pos_it = it->second.position_map.find(order_id);
            if (pos_it != it->second.position_map.end()) {
                return pos_it->second;
            }
        }
    }
    return 0; // Not found
}

size_t OrderBookFullDepth::queue_position(QueueId queue_id) const {
    // Look up the OrderId from the QueueId
    auto it = queue_id_to_order_id_.find(queue_id);
    if (it == queue_id_to_order_id_.end()) {
        return 0; // QueueId not found
    }
    
    OrderId order_id = it->second;
    
    // Search in bids
    const auto& bids = get_bids();
    for (const auto& pair : bids) {
        const auto& level = pair.second;
        auto pos_it = level.position_map.find(order_id);
        if (pos_it != level.position_map.end()) {
            return pos_it->second;
        }
    }
    
    // Search in asks
    const auto& asks = get_asks();
    for (const auto& pair : asks) {
        const auto& level = pair.second;
        auto pos_it = level.position_map.find(order_id);
        if (pos_it != level.position_map.end()) {
            return pos_it->second;
        }
    }
    
    return 0; // Not found
}

// --- Price Level Queries ---

std::vector<Price> OrderBookFullDepth::top_n_prices(Order::Side side, size_t n) const {
    std::vector<Price> prices;
    
    if (side == Order::Side::BUY) {
        const auto& bids = get_bids();
        for (const auto& pair : bids) {
            if (prices.size() >= n) break;
            prices.push_back(pair.first);
        }
    } else { // SELL
        const auto& asks = get_asks();
        for (const auto& pair : asks) {
            if (prices.size() >= n) break;
            prices.push_back(pair.first);
        }
    }
    
    return prices;
}

TopOfBook OrderBookFullDepth::top_of_book() const {
    TopOfBook tob;
    
    const auto& bids = get_bids();
    if (!bids.empty()) {
        tob.best_bid_price = bids.begin()->first;
        tob.best_bid_size = bids.begin()->second.total_size;
    }
    
    const auto& asks = get_asks();
    if (!asks.empty()) {
        tob.best_ask_price = asks.begin()->first;
        tob.best_ask_size = asks.begin()->second.total_size;
    }
    
    return tob;
}

// --- Legacy Interface (for backward compatibility) ---

void OrderBookFullDepth::on_tick(const Tick& tick) {
    // Refresh synthetic displayed liquidity from the tick's quote while
    // preserving resting strategy orders and their FIFO positions. Displayed
    // size is re-inserted at the FRONT of its level: with L1 data we cannot
    // see the real queue, so displayed liquidity is conservatively assumed to
    // be ahead of strategy orders resting at the same price. Queue progress
    // within a quote interval comes from trade prints consuming the level.
    remove_synthetic(Order::Side::BUY);
    remove_synthetic(Order::Side::SELL);
    if (tick.bid_size > 0) {
        synthetic_bid_qid_ = enqueue_order_front(Order::Side::BUY, tick.bid, "__quote_bid", tick.bid_size);
        synthetic_bid_price_ = tick.bid;
    }
    if (tick.ask_size > 0) {
        synthetic_ask_qid_ = enqueue_order_front(Order::Side::SELL, tick.ask, "__quote_ask", tick.ask_size);
        synthetic_ask_price_ = tick.ask;
    }
}

void OrderBookFullDepth::remove_synthetic(Order::Side side) {
    QueueId& qid = (side == Order::Side::BUY) ? synthetic_bid_qid_ : synthetic_ask_qid_;
    Price price = (side == Order::Side::BUY) ? synthetic_bid_price_ : synthetic_ask_price_;
    if (qid != 0) {
        // May already be gone if a trade print fully consumed it
        cancel_order(qid);
        remove_level_if_empty(side, price);
        qid = 0;
    }
}

Volume OrderBookFullDepth::consume_liquidity(Order::Side side, std::int64_t quantity) {
    return fill_market(side, quantity).first;
}

bool OrderBookFullDepth::cancel_order(QueueId queue_id) {
    // Look up the OrderId from the QueueId
    auto it = queue_id_to_order_id_.find(queue_id);
    if (it == queue_id_to_order_id_.end()) {
        return false; // QueueId not found
    }
    
    OrderId order_id = it->second;
    
    // Search in bids
    auto& bids = get_bids();
    for (auto& pair : bids) {
        auto& level = pair.second;
        auto pos_it = level.position_map.find(order_id);
        if (pos_it != level.position_map.end()) {
            // Found the order, remove it from queue
            auto queue_it = std::find(level.queue.begin(), level.queue.end(), order_id);
            if (queue_it != level.queue.end()) {
                size_t position = pos_it->second;
                level.queue.erase(queue_it);
                level.position_map.erase(pos_it);

                // Release the order's size from the level totals
                auto size_it = level.order_sizes.find(order_id);
                if (size_it != level.order_sizes.end()) {
                    level.total_size -= size_it->second;
                    level.order_sizes.erase(size_it);
                }
                
                // Update positions of orders behind this one
                for (auto& pos_pair : level.position_map) {
                    if (pos_pair.second > position) {
                        pos_pair.second--; // Decrement positions of orders behind
                    }
                }
                
                // Remove from queue_id_to_order_id_ mapping
                queue_id_to_order_id_.erase(it);
                return true;
            }
        }
    }
    
    // Search in asks
    auto& asks = get_asks();
    for (auto& pair : asks) {
        auto& level = pair.second;
        auto pos_it = level.position_map.find(order_id);
        if (pos_it != level.position_map.end()) {
            // Found the order, remove it from queue
            auto queue_it = std::find(level.queue.begin(), level.queue.end(), order_id);
            if (queue_it != level.queue.end()) {
                size_t position = pos_it->second;
                level.queue.erase(queue_it);
                level.position_map.erase(pos_it);

                // Release the order's size from the level totals
                auto size_it = level.order_sizes.find(order_id);
                if (size_it != level.order_sizes.end()) {
                    level.total_size -= size_it->second;
                    level.order_sizes.erase(size_it);
                }
                
                // Update positions of orders behind this one
                for (auto& pos_pair : level.position_map) {
                    if (pos_pair.second > position) {
                        pos_pair.second--; // Decrement positions of orders behind
                    }
                }
                
                // Remove from queue_id_to_order_id_ mapping
                queue_id_to_order_id_.erase(it);
                return true;
            }
        }
    }
    
    return false; // Order not found
}

std::pair<Volume, Price> OrderBookFullDepth::fill_market(Order::Side side, std::int64_t quantity) {
    if (quantity <= 0) {
        return {0, 0.0};
    }

    Volume total_filled = 0;
    double total_value = 0.0;
    Volume remaining_qty = static_cast<Volume>(quantity);

    // The resting side is the opposite of the aggressing order's side
    Order::Side resting_side = (side == Order::Side::BUY) ? Order::Side::SELL : Order::Side::BUY;

    // Both maps iterate best-price-first (asks ascending, bids descending),
    // so a single walk works for either side.
    auto walk_levels = [&](auto& levels) {
        auto it = levels.begin();
        while (it != levels.end() && remaining_qty > 0) {
            Price price = it->first;
            Level& level = it->second;

            while (!level.queue.empty() && remaining_qty > 0) {
                const OrderId& head_id = level.queue.front();
                auto size_it = level.order_sizes.find(head_id);
                Volume head_size = (size_it != level.order_sizes.end()) ? size_it->second : 0;

                if (head_size <= remaining_qty) {
                    // Fully consume the head order; dequeue_head keeps the
                    // queue, position map, and size totals in sync
                    dequeue_head(resting_side, price);
                    total_filled += head_size;
                    total_value += price * static_cast<double>(head_size);
                    remaining_qty -= head_size;
                } else {
                    // Partially consume the head order; it keeps its queue position
                    size_it->second -= remaining_qty;
                    level.total_size -= remaining_qty;
                    total_filled += remaining_qty;
                    total_value += price * static_cast<double>(remaining_qty);
                    remaining_qty = 0;
                }
            }

            // Remove emptied levels
            if (level.queue.empty()) {
                it = levels.erase(it);
            } else {
                ++it;
            }
        }
    };

    if (side == Order::Side::BUY) {
        walk_levels(get_asks());
    } else {
        walk_levels(get_bids());
    }

    // Calculate VWAP
    Price avg_price = (total_filled > 0) ? (total_value / static_cast<double>(total_filled)) : 0.0;

    return {total_filled, avg_price};
}

} // namespace qse 