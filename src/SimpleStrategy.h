#pragma once

#include "IStrategy.h"
#include "OrderManager.h"

namespace qse {

class SimpleStrategy : public IStrategy {
public:
    SimpleStrategy(OrderManager* order_manager)
        : order_manager_(order_manager), first_bar_(true) {}

    void on_bar(const Bar& bar) override {
        if (first_bar_) {
            // On the very first bar, execute a buy order.
            order_manager_->execute_buy(bar.close);
            first_bar_ = false;
        }
        // Do nothing on subsequent bars.
    }

private:
    OrderManager* order_manager_;
    bool first_bar_;
};

} // namespace qse 