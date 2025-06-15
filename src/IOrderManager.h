#pragma once

namespace qse {

class IOrderManager {
public:
    virtual ~IOrderManager() = default;
    virtual void execute_buy(double price) = 0;
    virtual void execute_sell(double price) = 0;
};

} // namespace qse 