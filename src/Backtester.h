#pragma once

#include "IDataReader.h"
#include "IStrategy.h"
#include "IOrderManager.h"
#include <memory>
#include <vector>

namespace qse {

class Backtester {
public:
    Backtester(
        std::unique_ptr<IDataReader> data_reader,
        std::unique_ptr<IStrategy> strategy,
        std::unique_ptr<IOrderManager> order_manager);

    void run();

private:
    std::unique_ptr<IDataReader> data_reader_;
    std::unique_ptr<IStrategy> strategy_;
    std::unique_ptr<IOrderManager> order_manager_;
};

} // namespace qse 