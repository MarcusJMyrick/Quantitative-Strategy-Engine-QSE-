#include "qse/order/OrderManager.h"
#include "qse/core/Debug.h"
#include "qse/data/Data.h" // Required for the Trade struct

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <algorithm>

namespace qse {

OrderManager::OrderManager(const Config& config, OrderBook& order_book, const std::string& equity_curve_path, const std::string& tradelog_path)
    : config_(&config), order_book_(&order_book), use_full_depth_(config.use_full_depth_book()),
      cash_(config.get_initial_cash()), next_order_id_(1) {

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

OrderManager::OrderManager(const Config& config, const std::string& equity_curve_path, const std::string& tradelog_path)
    : config_(&config), order_book_(nullptr), use_full_depth_(config.use_full_depth_book()),
      cash_(config.get_initial_cash()), next_order_id_(1) {

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

OrderManager::OrderManager(double initial_cash, const std::string& equity_curve_path, const std::string& tradelog_path)
    : config_(nullptr), order_book_(nullptr), cash_(initial_cash), next_order_id_(1) {

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

std::vector<Position> OrderManager::get_positions() const {
    std::vector<Position> result;
    for (const auto& pair : positions_) {
        if (pair.second != 0) {  // Only include non-zero positions
            result.emplace_back(pair.first, static_cast<double>(pair.second));
        }
    }
    return result;
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

    if (use_full_depth_) {
        // Any marketable portion takes liquidity immediately; the remainder
        // rests in the depth book behind the existing queue at its price
        Order& stored = orders_[order_id];
        Tick submit_tick{};
        submit_tick.symbol = symbol;
        submit_tick.timestamp = stored.timestamp;

        auto& book = depth_books_[symbol];
        take_liquidity(stored, book, submit_tick);

        if (stored.is_filled()) {
            remove_order_from_book(order_id);
        } else if (tif == Order::TimeInForce::IOC) {
            // IOC never rests: cancel whatever could not fill immediately
            stored.status = Order::Status::CANCELLED;
            remove_order_from_book(order_id);
        } else {
            QueueId qid = book.enqueue_order(side, limit_price, order_id, stored.remaining_quantity());
            limit_queue_ids_[order_id] = qid;
        }
    }

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

    // If the order rests in a depth book, pull it out of the FIFO queue
    auto qid_it = limit_queue_ids_.find(order_id);
    if (qid_it != limit_queue_ids_.end()) {
        auto db_it = depth_books_.find(order.symbol);
        if (db_it != depth_books_.end()) {
            db_it->second.cancel_order(qid_it->second);
            db_it->second.remove_level_if_empty(order.side, order.limit_price);
        }
        limit_queue_ids_.erase(qid_it);
    }

    remove_order_from_book(order_id);
    return true;
}

void OrderManager::process_tick(const Tick& tick) {
    // Update the order book first
    if (use_full_depth_) {
        // The tick's trade consumes the FIFO queue at its price (advancing
        // and possibly filling resting strategy orders) BEFORE the quote
        // refresh re-inserts synthetic displayed liquidity
        if (tick.volume > 0 && tick.price > 0) {
            consume_trade_print(tick);
        }
        depth_books_[tick.symbol].on_tick(tick);
    } else if (order_book_ != nullptr) {
        order_book_->on_tick(tick);
    }
    
    // Use the tick's symbol for order matching
    std::string symbol = tick.symbol;
    
    if (qse_debug_enabled()) std::cout << "DEBUG: Processing tick for " << symbol << " bid=" << tick.bid << " ask=" << tick.ask << " mid=" << tick.mid_price() << std::endl;
    
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
        if (qse_debug_enabled()) std::cout << "DEBUG: No orders found for symbol " << symbol << std::endl;
        return;
    }
    
    // Iterate a copy: maker fills triggered inside the loop can remove
    // entries from symbol_orders_ and invalidate references into it
    std::vector<OrderId> order_ids = symbol_it->second;
    if (qse_debug_enabled()) std::cout << "DEBUG: Found " << order_ids.size() << " orders for symbol " << symbol << std::endl;
    std::vector<OrderId> to_remove;
    
    // Get current top of book from whichever book model is active
    TopOfBook tob;
    if (use_full_depth_) {
        auto db_it = depth_books_.find(symbol);
        if (db_it != depth_books_.end()) {
            tob = db_it->second.top_of_book();
        }
    } else if (order_book_ != nullptr) {
        tob = order_book_->top_of_book(symbol);
    }
    
    for (const OrderId& order_id : order_ids) {
        auto order_it = orders_.find(order_id);
        if (order_it == orders_.end()) {
            continue;
        }
        
        Order& order = order_it->second;
        if (!order.is_active()) {
            if (qse_debug_enabled()) std::cout << "DEBUG: Order " << order_id << " is not active, status=" << static_cast<int>(order.status) << std::endl;
            continue;
        }
        
        if (qse_debug_enabled()) std::cout << "DEBUG: Processing order " << order_id << " type=" << static_cast<int>(order.type) << " side=" << static_cast<int>(order.side) << " qty=" << order.quantity << std::endl;
        
        Volume fill_qty = 0;
        Price fill_price = 0.0;
        bool depth_fill = false;

        if (use_full_depth_) {
            if (order.type == Order::Type::MARKET) {
                // Walk the full-depth book: the taker pays the VWAP of the
                // liquidity consumed, and any strategy maker orders that
                // were consumed get credited with their fills
                auto db_it = depth_books_.find(symbol);
                if (db_it != depth_books_.end()) {
                    take_liquidity(order, db_it->second, tick);
                    if (order.is_filled()) {
                        to_remove.push_back(order_id);
                    }
                }
            }
            // LIMIT orders rest in the depth book; their fills come from
            // trade prints consuming the queue (consume_trade_print)
            continue;
        }

        if (order.type == Order::Type::MARKET) {
            if (order_book_ != nullptr) {
                // Use OrderBook to consume liquidity
                fill_qty = order_book_->consume_liquidity(symbol, order.side, order.remaining_quantity());
                if (fill_qty > 0) {
                    fill_price = (order.side == Order::Side::BUY) ? tob.best_ask_price : tob.best_bid_price;
                }
            } else {
                // Fallback to old logic
                fill_qty = std::min(order.remaining_quantity(), tick.volume);
                fill_price = tick.mid_price();
            }
            if (qse_debug_enabled()) std::cout << "DEBUG: Market order fill_qty=" << fill_qty << " at " << fill_price << std::endl;
        } else if (order.type == Order::Type::LIMIT) {
            // Use OrderBook for limit order fills
            if (order_book_ != nullptr) {
                fill_qty = process_limit_order_fill(order, tob);
                if (fill_qty > 0) {
                    fill_price = (order.side == Order::Side::BUY) ? tob.best_ask_price : tob.best_bid_price;
                }
            } else {
                // Fallback to old logic
                if (order.side == Order::Side::BUY && tick.ask <= order.limit_price) {
                    fill_qty = std::min(order.remaining_quantity(), tick.volume);
                    fill_price = order.limit_price;
                } else if (order.side == Order::Side::SELL && tick.bid >= order.limit_price) {
                    fill_qty = std::min(order.remaining_quantity(), tick.volume);
                    fill_price = order.limit_price;
                }
            }
            if (qse_debug_enabled()) std::cout << "DEBUG: Limit order fill_qty=" << fill_qty << " at " << fill_price << std::endl;
        }
        
        if (fill_qty > 0) {
            if (qse_debug_enabled()) std::cout << "DEBUG: Filling order with qty=" << fill_qty << " at price=" << fill_price << std::endl;
            fill_order(order, fill_qty, fill_price, tick, depth_fill);

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

void OrderManager::fill_order(Order& order, Volume fill_qty, Price fill_price, const Tick& tick,
                              bool price_includes_impact) {
    // Calculate base fill price (before slippage)
    Price base_fill_price = fill_price;

    // Apply the linear slippage coefficient only when the fill price does not
    // already carry impact from walking the depth book
    if (config_ != nullptr && !price_includes_impact) {
        double k = config_->get_slippage_coeff(order.symbol);
        if (k > 0.0) {
            double slippage = base_fill_price * k * fill_qty;
            if (order.side == Order::Side::BUY) {
                base_fill_price += slippage;
            } else { // SELL
                base_fill_price -= slippage;
            }
            if (qse_debug_enabled()) std::cout << "DEBUG: Applied slippage for " << order.symbol
                      << " - base: " << fill_price << ", slippage: " << slippage
                      << ", final: " << base_fill_price << std::endl;
        }
    }
    
    // Update order
    Volume old_filled = order.filled_quantity;
    order.filled_quantity += fill_qty;
    
    // Update average fill price
    double total_cost = order.avg_fill_price * old_filled + base_fill_price * fill_qty;
    order.avg_fill_price = total_cost / order.filled_quantity;
    
    // Update status
    if (order.filled_quantity >= order.quantity) {
        order.status = Order::Status::FILLED;
    } else {
        order.status = Order::Status::PARTIALLY_FILLED;
    }
    
    // Update portfolio
    if (order.side == Order::Side::BUY) {
        double cost = fill_qty * base_fill_price;
        if (cost <= cash_) {
            cash_ -= cost;
            positions_[order.symbol] += fill_qty;
            log_trade(to_unix_ms(tick.timestamp), order.symbol, "BUY", fill_qty, base_fill_price);
        }
    } else { // SELL
        double proceeds = fill_qty * base_fill_price;
        cash_ += proceeds;
        positions_[order.symbol] -= fill_qty;
        log_trade(to_unix_ms(tick.timestamp), order.symbol, "SELL", fill_qty, base_fill_price);
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

Volume OrderManager::process_limit_order_fill(Order& order, const TopOfBook& tob) {
    Volume fill_qty = 0;
    
    if (order.side == Order::Side::BUY) {
        // Buy limit order fills when ask price is at or below limit price
        if (tob.has_ask() && tob.best_ask_price <= order.limit_price) {
            fill_qty = order_book_->consume_liquidity(order.symbol, order.side, order.remaining_quantity());
        }
    } else { // SELL
        // Sell limit order fills when bid price is at or above limit price
        if (tob.has_bid() && tob.best_bid_price >= order.limit_price) {
            fill_qty = order_book_->consume_liquidity(order.symbol, order.side, order.remaining_quantity());
        }
    }
    
    return fill_qty;
}

Volume OrderManager::process_ioc_order_fill(Order& order, const TopOfBook& tob) {
    Volume fill_qty = 0;
    
    if (order.side == Order::Side::BUY) {
        // IOC buy order fills when ask price is at or below limit price
        if (tob.has_ask() && tob.best_ask_price <= order.limit_price) {
            fill_qty = order_book_->consume_liquidity(order.symbol, order.side, order.remaining_quantity());
        }
    } else { // SELL
        // IOC sell order fills when bid price is at or above limit price
        if (tob.has_bid() && tob.best_bid_price >= order.limit_price) {
            fill_qty = order_book_->consume_liquidity(order.symbol, order.side, order.remaining_quantity());
        }
    }
    
    return fill_qty;
}

// --- NEW: Attempt fills against current order book ---
void OrderManager::attempt_fills() {
    if (order_book_ == nullptr && !use_full_depth_) {
        return; // No book model available
    }

    // Snapshot symbols first: removing filled orders can erase entries from
    // symbol_orders_, which would invalidate an iterator over it
    std::vector<std::string> symbols;
    symbols.reserve(symbol_orders_.size());
    for (const auto& symbol_pair : symbol_orders_) {
        symbols.push_back(symbol_pair.first);
    }

    // Process fills for each symbol that has active orders
    for (const std::string& symbol : symbols) {
        auto symbol_it = symbol_orders_.find(symbol);
        if (symbol_it == symbol_orders_.end() || symbol_it->second.empty()) {
            continue;
        }

        // Get current top of book from whichever book model is active
        TopOfBook tob;
        OrderBookFullDepth* depth = nullptr;
        if (use_full_depth_) {
            auto db_it = depth_books_.find(symbol);
            if (db_it == depth_books_.end()) {
                continue; // No liquidity known for this symbol
            }
            depth = &db_it->second;
            tob = depth->top_of_book();
        } else {
            tob = order_book_->top_of_book(symbol);
        }
        if (!tob.has_bid() && !tob.has_ask()) {
            continue; // No liquidity available
        }

        std::vector<OrderId> to_remove;

        // Iterate a copy: maker fills triggered inside the loop can remove
        // entries from symbol_orders_ and invalidate references into it
        std::vector<OrderId> order_ids = symbol_it->second;
        for (const OrderId& order_id : order_ids) {
            auto order_it = orders_.find(order_id);
            if (order_it == orders_.end()) {
                continue;
            }

            Order& order = order_it->second;
            if (!order.is_active()) {
                continue;
            }

            if (depth != nullptr) {
                if (order.type == Order::Type::MARKET) {
                    // Walk the full-depth book: the taker pays the VWAP of
                    // consumed liquidity, and any strategy maker orders that
                    // were consumed get credited with their fills
                    Tick book_tick{};
                    book_tick.symbol = symbol;
                    book_tick.timestamp = std::chrono::system_clock::now();
                    book_tick.bid = tob.best_bid_price;
                    book_tick.ask = tob.best_ask_price;
                    book_tick.bid_size = tob.best_bid_size;
                    book_tick.ask_size = tob.best_ask_size;
                    book_tick.price = tob.mid_price();

                    take_liquidity(order, *depth, book_tick);
                    if (order.is_filled()) {
                        to_remove.push_back(order_id);
                    }
                }
                // LIMIT orders rest in the depth book; their fills come from
                // trade prints consuming the queue (consume_trade_print)
                continue;
            }

            Volume fill_qty = 0;
            Price fill_price = 0.0;

            if (order.type == Order::Type::MARKET) {
                // Market orders fill immediately at the touch
                fill_qty = order_book_->consume_liquidity(symbol, order.side, order.remaining_quantity());
                if (fill_qty > 0) {
                    fill_price = (order.side == Order::Side::BUY) ? tob.best_ask_price : tob.best_bid_price;
                }
            } else if (order.type == Order::Type::LIMIT) {
                // Limit orders fill when price crosses limit
                fill_qty = process_limit_order_fill(order, tob);
                if (fill_qty > 0) {
                    fill_price = (order.side == Order::Side::BUY) ? tob.best_ask_price : tob.best_bid_price;
                }
            }

            if (fill_qty > 0) {
                // Create a dummy tick for the fill (since we don't have a real tick here)
                Tick dummy_tick;
                dummy_tick.symbol = symbol;
                dummy_tick.timestamp = std::chrono::system_clock::now();
                dummy_tick.bid = tob.best_bid_price;
                dummy_tick.ask = tob.best_ask_price;
                dummy_tick.bid_size = tob.best_bid_size;
                dummy_tick.ask_size = tob.best_ask_size;
                dummy_tick.price = (tob.best_bid_price + tob.best_ask_price) / 2.0;
                dummy_tick.volume = fill_qty;

                fill_order(order, fill_qty, fill_price, dummy_tick);

                // Emit fill callback if registered
                if (fill_callback_) {
                    std::string side_str = (order.side == Order::Side::BUY) ? "BUY" : "SELL";
                    Fill fill(order.order_id, order.symbol, fill_qty, fill_price, dummy_tick.timestamp, side_str);
                    fill_callback_(fill);
                }

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
}

// --- NEW: Set fill callback for strategy notifications ---
void OrderManager::set_fill_callback(FillCallback callback) {
    fill_callback_ = callback;
}

// --- Full-depth fill model helpers ---

Volume OrderManager::take_liquidity(Order& taker, OrderBookFullDepth& book, const Tick& tick) {
    Order::Side maker_side = (taker.side == Order::Side::BUY) ? Order::Side::SELL : Order::Side::BUY;
    Volume want = taker.remaining_quantity();
    Volume total = 0;
    double value = 0.0;

    while (total < want) {
        TopOfBook tob = book.top_of_book();
        Price level_price = 0.0;
        bool crossable = false;
        if (taker.side == Order::Side::BUY) {
            crossable = tob.has_ask() &&
                        (taker.type == Order::Type::MARKET || tob.best_ask_price <= taker.limit_price);
            level_price = tob.best_ask_price;
        } else {
            crossable = tob.has_bid() &&
                        (taker.type == Order::Type::MARKET || tob.best_bid_price >= taker.limit_price);
            level_price = tob.best_bid_price;
        }
        if (!crossable) {
            break;
        }

        auto consumed = book.consume_at_price(maker_side, level_price, want - total);
        if (consumed.empty()) {
            break;
        }
        for (const auto& entry : consumed) {
            total += entry.second;
            value += level_price * static_cast<double>(entry.second);
            apply_maker_fill(entry.first, entry.second, level_price, tick);
        }
    }

    if (total > 0) {
        Price vwap = value / static_cast<double>(total);
        fill_order(taker, total, vwap, tick, /*price_includes_impact=*/true);
        if (fill_callback_) {
            std::string side_str = (taker.side == Order::Side::BUY) ? "BUY" : "SELL";
            Fill fill(taker.order_id, taker.symbol, total, vwap, tick.timestamp, side_str);
            fill_callback_(fill);
        }
    }
    return total;
}

void OrderManager::consume_trade_print(const Tick& tick) {
    auto db_it = depth_books_.find(tick.symbol);
    if (db_it == depth_books_.end()) {
        return;
    }
    auto& book = db_it->second;

    // A trade print consumes the FIFO queue resting at its price. L1 data
    // carries no aggressor flag, so if both sides rest at the price the ask
    // side is consumed (deterministic tiebreak).
    Order::Side resting_side;
    if (book.has_level(Order::Side::SELL, tick.price)) {
        resting_side = Order::Side::SELL;
    } else if (book.has_level(Order::Side::BUY, tick.price)) {
        resting_side = Order::Side::BUY;
    } else {
        return; // Nothing resting at the trade price
    }

    auto consumed = book.consume_at_price(resting_side, tick.price, tick.volume);
    for (const auto& entry : consumed) {
        apply_maker_fill(entry.first, entry.second, tick.price, tick);
    }
}

void OrderManager::apply_maker_fill(const OrderId& maker_id, Volume qty, Price price, const Tick& tick) {
    auto it = orders_.find(maker_id);
    if (it == orders_.end()) {
        return; // Synthetic quote liquidity or externally seeded orders
    }

    Order& order = it->second;
    // Passive fills execute at the resting price with no taker impact
    fill_order(order, qty, price, tick, /*price_includes_impact=*/true);

    if (fill_callback_) {
        std::string side_str = (order.side == Order::Side::BUY) ? "BUY" : "SELL";
        Fill fill(order.order_id, order.symbol, qty, price, tick.timestamp, side_str);
        fill_callback_(fill);
    }

    if (order.is_filled()) {
        limit_queue_ids_.erase(maker_id);
        remove_order_from_book(maker_id);
    }
}

} // namespace qse