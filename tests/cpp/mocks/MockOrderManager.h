#pragma once

#include <gmock/gmock.h>
#include "qse/data/Data.h"           // For qse::Trade
#include "qse/order/IOrderManager.h"

class MockOrderManager : public qse::IOrderManager {
public:
    MOCK_METHOD(void, execute_buy, (const qse::Bar& bar), (override));
    MOCK_METHOD(void, execute_sell, (const qse::Bar& bar), (override));
    MOCK_METHOD(int, get_position, (), (const, override));
    MOCK_METHOD(double, get_portfolio_value, (double current_price), (const, override));
    MOCK_METHOD(const std::vector<qse::Trade>&, get_trade_log, (), (const, override));
}; 