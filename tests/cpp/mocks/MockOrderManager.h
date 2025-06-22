#pragma once

#include "gmock/gmock.h"
#include "qse/order/IOrderManager.h"
#include <string>
#include <map>

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
}; 