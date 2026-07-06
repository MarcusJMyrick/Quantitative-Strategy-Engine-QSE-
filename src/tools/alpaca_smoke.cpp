// Manual paper-trading smoke test (roadmap E2 done-when): places one 1-share
// limit order far below the market on the Alpaca PAPER account, verifies it
// via GET, cancels it, and verifies the cancel - each step visible in the
// Alpaca dashboard. Never run in CI; requires explicit --paper plus
// APCA_API_KEY_ID / APCA_API_SECRET_KEY in the environment.
//
//   APCA_API_KEY_ID=... APCA_API_SECRET_KEY=... ./build/alpaca_smoke --paper

#include "qse/exe/AlpacaExecutionHandler.h"
#include "qse/exe/CurlHttpClient.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "--paper") {
        std::cerr << "This tool talks to your Alpaca PAPER account.\n"
                  << "Run explicitly with: alpaca_smoke --paper\n";
        return 1;
    }

    try {
        auto handler =
            qse::AlpacaExecutionHandler::from_env(std::make_shared<qse::CurlHttpClient>());

        // 1-share buy limit at $1.00: cannot fill, sits visibly on the book
        std::cout << "Submitting 1-share AAPL buy limit @ $1.00 (day)...\n";
        qse::OrderId id = handler.submit_limit_order("AAPL", qse::Order::Side::BUY, 1, 1.0,
                                                     qse::Order::TimeInForce::DAY);
        if (id.empty()) {
            std::cerr << "Order rejected - check credentials/market status\n";
            return 1;
        }
        std::cout << "  accepted, order id: " << id << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (auto order = handler.get_order(id)) {
            std::cout << "  status: " << static_cast<int>(order->status)
                      << " (0=PENDING expected), qty " << order->quantity << " @ $"
                      << order->limit_price << "\n";
        }

        std::cout << "Cancelling...\n";
        if (!handler.cancel_order(id)) {
            std::cerr << "Cancel failed - check the dashboard\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (auto order = handler.get_order(id)) {
            std::cout << "  status: " << static_cast<int>(order->status)
                      << " (3=CANCELLED expected)\n";
        }

        std::cout << "Smoke test complete - verify both events in the Alpaca dashboard\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
