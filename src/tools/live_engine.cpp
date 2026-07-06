// Live paper-trading session (roadmap E3): Alpaca REST quote feed -> SPSC
// ring -> BarBuilder -> SMA crossover -> AlpacaExecutionHandler, with a
// local order/fill log and an end-of-session reconciliation against the
// venue. PAPER ONLY - requires --paper plus APCA_API_KEY_ID /
// APCA_API_SECRET_KEY in the environment.
//
//   set -a; source .env; set +a
//   ./build/live_engine --paper --symbol AAPL --minutes 10 --bar-seconds 60 \
//       --short 3 --long 8 --size 1
//
// Short windows (e.g. 3/8 on 60s bars) make a crossover likely within a
// 10-minute session; run during US market hours so orders actually fill.

#include "qse/exe/AlpacaExecutionHandler.h"
#include "qse/exe/CurlHttpClient.h"
#include "qse/live/AlpacaMarketDataFeed.h"
#include "qse/live/LiveEngine.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
qse::LiveEngine* g_engine = nullptr;
void handle_sigint(int) {
    if (g_engine != nullptr) {
        g_engine->stop();
    }
}
} // namespace

int main(int argc, char** argv) {
    std::string symbol = "AAPL";
    int minutes = 10;
    int bar_seconds = 60;
    std::size_t sma_short = 3;
    std::size_t sma_long = 8;
    qse::Volume size = 1;
    bool paper = false;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--paper") {
            paper = true;
        } else if (i + 1 < argc) {
            std::string value = argv[++i];
            if (flag == "--symbol")
                symbol = value;
            else if (flag == "--minutes")
                minutes = std::stoi(value);
            else if (flag == "--bar-seconds")
                bar_seconds = std::stoi(value);
            else if (flag == "--short")
                sma_short = std::stoul(value);
            else if (flag == "--long")
                sma_long = std::stoul(value);
            else if (flag == "--size")
                size = std::stoll(value);
            else {
                std::cerr << "Unknown flag: " << flag << "\n";
                return 1;
            }
        }
    }
    if (!paper) {
        std::cerr << "This tool trades on your Alpaca PAPER account.\n"
                  << "Run explicitly with: live_engine --paper [options]\n";
        return 1;
    }

    try {
        auto http = std::make_shared<qse::CurlHttpClient>();
        auto exec = qse::AlpacaExecutionHandler::from_env(http);

        const char* key_id = std::getenv("APCA_API_KEY_ID");
        const char* secret = std::getenv("APCA_API_SECRET_KEY");
        qse::AlpacaMarketDataFeed feed(http, key_id, secret, symbol,
                                       std::chrono::milliseconds(1000));

        qse::LiveEngineConfig config;
        config.symbol = symbol;
        config.bar_interval = std::chrono::seconds(bar_seconds);
        config.sma_short = sma_short;
        config.sma_long = sma_long;
        config.order_size = size;

        qse::LiveEngine engine(config, exec,
                               [&feed](const auto& handler) { return feed.drain(handler); });
        g_engine = &engine;
        std::signal(SIGINT, handle_sigint);

        std::cout << "Live session: " << symbol << ", " << minutes << " min, SMA " << sma_short
                  << "/" << sma_long << " on " << bar_seconds << "s bars, size " << size
                  << " (Ctrl-C to stop early)\n";
        feed.start();
        engine.run_for(std::chrono::minutes(minutes));
        feed.stop();

        engine.save_logs("results/live");
        std::cout << "\nSession done: " << engine.bars_seen() << " bars, "
                  << engine.submitted_orders().size() << " orders, " << engine.local_fills().size()
                  << " local fills, " << feed.dropped_ticks()
                  << " dropped ticks. Logs in results/live/\n";

        auto report = engine.reconcile();
        std::cout << "\nReconciliation vs Alpaca:\n";
        for (const auto& line : report.lines) {
            std::cout << "  " << line.order_id << ": local " << line.local_fill_qty << " / venue "
                      << line.venue_fill_qty << (line.matched ? "  MATCH" : "  MISMATCH") << "\n";
        }
        if (report.lines.empty()) {
            std::cout << "  (no orders this session - try shorter SMA windows or a longer run)\n";
        }
        std::cout << (report.all_matched ? "RECONCILED: local log matches the venue\n"
                                         : "MISMATCH: investigate before trusting the log\n");
        return report.all_matched ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
