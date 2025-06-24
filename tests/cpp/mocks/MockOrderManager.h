#pragma once

#include "gmock/gmock.h"
#include "qse/order/IOrderManager.h"
#include <string>
#include <map>
#include <optional>
#include <vector>

// A mock implementation of the IOrderManager interface for use in tests.
// This allows us to isolate strategies and verify their behavior.
class MockOrderManager : public qse::IOrderManager {
public:
    using MarketPrices = std::map<std::string, double>;
    
    MOCK_METHOD(void, execute_buy, (const std::string& symbol, int quantity, double price), (override));
    MOCK_METHOD(void, execute_sell, (const std::string& symbol, int quantity, double price), (override));
    MOCK_METHOD(int, get_position, (const std::string& symbol), (const, override));
    MOCK_METHOD(double, get_cash, (), (const, override));
    MOCK_METHOD(void, record_equity, (long long timestamp, const MarketPrices& market_prices), (override));
    
    // --- Tick-level order management mock methods ---
    MOCK_METHOD(qse::OrderId, submit_market_order, (const std::string& symbol, qse::Order::Side side, qse::Volume quantity), (override));
    MOCK_METHOD(qse::OrderId, submit_limit_order, (const std::string& symbol, qse::Order::Side side, qse::Volume quantity, qse::Price limit_price, qse::Order::TimeInForce tif), (override));
    MOCK_METHOD(bool, cancel_order, (const qse::OrderId& order_id), (override));
    MOCK_METHOD(void, process_tick, (const qse::Tick& tick), (override));
    
    // --- NEW: Fill callback mock methods ---
    MOCK_METHOD(void, attempt_fills, (), (override));
    MOCK_METHOD(void, set_fill_callback, (qse::IOrderManager::FillCallback callback), (override));
    
    MOCK_METHOD(std::optional<qse::Order>, get_order, (const qse::OrderId& order_id), (const, override));
    MOCK_METHOD(std::vector<qse::Order>, get_active_orders, (const std::string& symbol), (const, override));
}; 