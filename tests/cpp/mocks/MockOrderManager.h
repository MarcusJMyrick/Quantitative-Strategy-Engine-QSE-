#pragma once

#include "qse/order/IOrderManager.h"
#include <gmock/gmock.h>
#include <string>
#include <map>

namespace qse {

class MockOrderManager : public IOrderManager {
public:
    MOCK_METHOD(void, execute_buy, (const std::string& symbol, int quantity, double price), (override));
    MOCK_METHOD(void, execute_sell, (const std::string& symbol, int quantity, double price), (override));
    MOCK_METHOD(int, get_position, (const std::string& symbol), (const, override));
    MOCK_METHOD(double, get_cash, (), (const, override));
    MOCK_METHOD(void, record_equity, (long long timestamp, (const std::map<std::string, double>&) market_prices), (override));
};

} // namespace qse 