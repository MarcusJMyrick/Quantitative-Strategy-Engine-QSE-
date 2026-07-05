// A/B slippage audit driver (roadmap H1): runs the exact same SMA signal
// stream through two OrderManager configurations and writes paired equity
// curves + tradelogs per order-size regime.
//
//   Engine A (naive): legacy fill model - market orders fill at the tick MID,
//     no spread, no impact, no queue (capped per tick by displayed volume).
//   Engine B (depth): full-depth book - orders pay the ask/bid and walk the
//     seeded depth profile, receiving the true VWAP of consumed liquidity.
//
// Ticks carry trades only (timestamp,price,volume), so the quote is
// synthesized with a one-tick half-spread and, in depth mode, a uniform
// profile of N levels behind the touch is re-seeded at every tick (each level
// as thick as the displayed touch volume - the profile validated in the
// impact study, docs/research/microstructure/).
//
// Output per (mode, size): results/ab_audit/{equity,tradelog}_<mode>_<size>.csv
// Analyzed by scripts/analysis/slippage_audit.py.

#include "qse/order/OrderManager.h"
#include "qse/data/OrderBookFullDepth.h"
#include "qse/strategy/MovingAverage.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kHalfSpread = 0.01;   // synthetic quote half-spread ($)
constexpr double kTickSize = 0.01;     // price grid behind the touch ($)
constexpr std::size_t kSeedLevels = 12; // depth levels seeded behind the touch
constexpr double kInitialCash = 20'000'000.0; // large enough that the cash
                                              // guard never distorts fills
constexpr std::size_t kShortWindow = 20;
constexpr std::size_t kLongWindow = 50;

struct Row {
    long long ts_ms = 0;
    double price = 0.0;
    long long volume = 0;
};

std::vector<Row> load_rows(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open tick file: " + path);
    }
    std::vector<Row> rows;
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string ts, price, volume;
        if (!std::getline(ss, ts, ',') || !std::getline(ss, price, ',') ||
            !std::getline(ss, volume, ',')) {
            continue;
        }
        try {
            Row r;
            r.ts_ms = std::stoll(ts);
            if (r.ts_ms < 10'000'000'000LL) r.ts_ms *= 1000; // seconds -> ms
            r.price = std::stod(price);
            r.volume = std::stoll(volume);
            if (r.price > 0.0 && r.volume > 0) {
                rows.push_back(r);
            }
        } catch (const std::exception&) {
            // Skip malformed rows
        }
    }
    return rows;
}

struct Seed {
    qse::Order::Side side;
    qse::Price price;
    qse::QueueId qid;
};

void run_one(bool depth_mode, qse::Volume order_size,
             const std::vector<Row>& rows, const std::string& symbol,
             const std::string& out_prefix) {
    qse::OrderManager om(kInitialCash, out_prefix + "_equity.csv",
                         out_prefix + "_tradelog.csv");
    om.set_use_full_depth(depth_mode);

    qse::MovingAverage short_ma(kShortWindow);
    qse::MovingAverage long_ma(kLongWindow);

    // Bars are built inline (ticks arrive on a fixed 60s grid); the SMA state
    // updates once per completed bar exactly as SMACrossoverStrategy does
    std::vector<Seed> seeds;
    bool is_long = false;
    long long trades = 0;

    for (const Row& row : rows) {
        qse::Tick tick;
        tick.symbol = symbol;
        tick.timestamp = qse::Timestamp(std::chrono::milliseconds(row.ts_ms));
        tick.price = row.price;
        tick.volume = static_cast<qse::Volume>(row.volume);
        tick.bid = row.price - kHalfSpread;
        tick.ask = row.price + kHalfSpread;
        tick.bid_size = static_cast<qse::Volume>(row.volume);
        tick.ask_size = static_cast<qse::Volume>(row.volume);

        // 1) Signal: one bar per tick (the file is a 1-minute grid), same
        //    crossover rule for both engines - long on golden, flat on death
        double prev_short = short_ma.get_value();
        double prev_long = long_ma.get_value();
        short_ma.update(row.price);
        long_ma.update(row.price);
        if (long_ma.is_ready() && prev_long > 0.0) {
            double cur_short = short_ma.get_value();
            double cur_long = long_ma.get_value();
            if (prev_short < prev_long && cur_short > cur_long && !is_long) {
                om.submit_market_order(symbol, qse::Order::Side::BUY, order_size);
                is_long = true;
                ++trades;
            } else if (prev_short > prev_long && cur_short < cur_long && is_long) {
                om.submit_market_order(symbol, qse::Order::Side::SELL, order_size);
                is_long = false;
                ++trades;
            }
        }

        // 2) Depth mode: re-seed the uniform profile behind this tick's touch
        //    before the book refresh, so fills see current prices
        if (depth_mode) {
            auto& book = om.depth_book(symbol);
            for (const Seed& s : seeds) {
                book.cancel_order(s.qid);
                book.remove_level_if_empty(s.side, s.price);
            }
            seeds.clear();
            for (std::size_t lvl = 1; lvl <= kSeedLevels; ++lvl) {
                qse::Price ask_px = tick.ask + static_cast<double>(lvl) * kTickSize;
                qse::Price bid_px = tick.bid - static_cast<double>(lvl) * kTickSize;
                qse::QueueId aq = book.enqueue_order(
                    qse::Order::Side::SELL, ask_px,
                    "seed_a" + std::to_string(lvl), tick.ask_size);
                qse::QueueId bq = book.enqueue_order(
                    qse::Order::Side::BUY, bid_px,
                    "seed_b" + std::to_string(lvl), tick.bid_size);
                seeds.push_back({qse::Order::Side::SELL, ask_px, aq});
                seeds.push_back({qse::Order::Side::BUY, bid_px, bq});
            }
        }

        // 3) Fill: naive matches at this tick's mid inside process_tick;
        //    depth walks the freshly seeded book
        om.process_tick(tick);
        om.attempt_fills();

        // 4) Mark to market at the mid for both engines
        om.record_equity(row.ts_ms, {{symbol, row.price}});
    }

    double final_equity = om.get_cash() +
        om.get_position(symbol) * rows.back().price;
    std::cout << (depth_mode ? "depth" : "naive") << " size " << order_size
              << ": " << trades << " signals, final equity "
              << std::fixed << final_equity << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string ticks_path = "data/raw_ticks_AAPL.csv";
    std::string out_dir = "results/ab_audit";
    std::string symbol = "AAPL";
    std::vector<qse::Volume> sizes = {1000, 5000, 25000};

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        std::string value = argv[i + 1];
        if (flag == "--ticks") ticks_path = value;
        else if (flag == "--out-dir") out_dir = value;
        else if (flag == "--symbol") symbol = value;
        else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    std::vector<Row> rows;
    try {
        rows = load_rows(ticks_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    if (rows.size() < kLongWindow + 2) {
        std::cerr << "Not enough ticks in " << ticks_path << "\n";
        return 1;
    }

    std::filesystem::create_directories(out_dir);

    for (qse::Volume size : sizes) {
        for (bool depth_mode : {false, true}) {
            std::string prefix = out_dir + "/" +
                (depth_mode ? "depth" : "naive") + "_" + std::to_string(size);
            run_one(depth_mode, size, rows, symbol, prefix);
        }
    }

    std::cout << "Audit runs written to " << out_dir << "\n";
    return 0;
}
