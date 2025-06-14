#pragma once
#include "Data.h"
#include <vector>
#include <queue>

class OrderManager {
public:
    void placeOrder(const Order& order);
    void processOrders(const MarketData& current_data);
    std::vector<Order> getFilledOrders() const;
    
private:
    std::queue<Order> pending_orders_;
    std::vector<Order> filled_orders_;
}; 