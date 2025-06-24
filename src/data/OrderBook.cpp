#include "qse/data/OrderBook.h"
#include <iostream>

namespace qse {

void OrderBook::on_tick(const Tick& tick) {
    auto& tob = books_[tick.symbol];
    tob.best_bid_price = tick.bid;
    tob.best_bid_size = tick.bid_size;
    tob.best_ask_price = tick.ask;
    tob.best_ask_size = tick.ask_size;
}

const TopOfBook& OrderBook::top_of_book(const std::string& symbol) const {
    static TopOfBook empty;
    auto it = books_.find(symbol);
    return (it != books_.end()) ? it->second : empty;
}

Volume OrderBook::consume_liquidity(const std::string& symbol, Order::Side side, Volume quantity) {
    auto it = books_.find(symbol);
    if (it == books_.end()) {
        return 0; // No liquidity available
    }
    
    TopOfBook& tob = it->second;
    Volume consumed = 0;
    
    if (side == Order::Side::BUY) {
        // Buy orders consume ask liquidity
        if (tob.has_ask()) {
            consumed = std::min(quantity, tob.best_ask_size);
            tob.best_ask_size -= consumed;
        }
    } else { // SELL
        // Sell orders consume bid liquidity
        if (tob.has_bid()) {
            consumed = std::min(quantity, tob.best_bid_size);
            tob.best_bid_size -= consumed;
        }
    }
    
    return consumed;
}

} // namespace qse 