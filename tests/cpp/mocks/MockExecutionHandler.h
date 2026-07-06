#pragma once

#include <gmock/gmock.h>
#include "qse/exe/IExecutionHandler.h"

namespace qse {

class MockExecutionHandler : public IExecutionHandler {
public:
    MOCK_METHOD(OrderId, submit_market_order,
                (const std::string& symbol, Order::Side side, Volume quantity), (override));
    MOCK_METHOD(OrderId, submit_limit_order,
                (const std::string& symbol, Order::Side side, Volume quantity, Price limit_price,
                 Order::TimeInForce tif),
                (override));
    MOCK_METHOD(bool, cancel_order, (const OrderId& order_id), (override));
    MOCK_METHOD(OrderId, replace_order,
                (const OrderId& order_id, Volume new_quantity, Price new_limit_price), (override));
    MOCK_METHOD(std::optional<Order>, get_order, (const OrderId& order_id), (const, override));
    MOCK_METHOD(void, set_fill_callback, (FillCallback callback), (override));
    MOCK_METHOD(std::size_t, poll_fills, (), (override));
};

} // namespace qse
