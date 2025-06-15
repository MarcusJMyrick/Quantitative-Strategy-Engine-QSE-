#include "DataReader.h"
#include "OrderManager.h"
#include "SimpleStrategy.h"
#include "Backtester.h"
#include <iostream>
#include <memory>

int main() {
    try {
        // 1. Set up the components
        auto data_reader = std::make_unique<qse::DataReader>("./test_data/test_bars.parquet");
        auto order_manager = std::make_unique<qse::OrderManager>();
        
        // The strategy needs a pointer to the order manager to place trades
        auto strategy = std::make_unique<qse::SimpleStrategy>(order_manager.get());

        // 2. Create the backtester with the components
        qse::Backtester backtester(
            std::move(data_reader),
            std::move(strategy),
            std::move(order_manager)
        );

        // 3. Run the simulation!
        backtester.run();

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 