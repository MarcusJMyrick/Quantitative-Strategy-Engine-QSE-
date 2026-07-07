// QR4.7 — multi-symbol daily A/B slippage audit for the eigenportfolio stat
// arb and its baselines (QR4.5/QR4.6). The daily-frequency analogue of the H1
// audit (src/tools/ab_audit.cpp): the SAME dollar-neutral weight files run
// through two OrderManager fill models across order-size regimes.
//
//   Engine A (naive): market orders fill at the tick MID, infinite liquidity
//     (tick.volume is set huge so the cap never binds) - the standard
//     backtester assumption.
//   Engine B (depth): market orders walk the full-depth book and pay the VWAP
//     of the liquidity consumed. Since we have only daily bars for the 15-name
//     universe (no intraday L2), each symbol's book is re-seeded every day with
//     a uniform depth profile whose total visible size is a fraction of that
//     day's volume (scaled from IEX-partial to a consolidated ADV estimate).
//     This is a STATED APPROXIMATION - the honest analogue of H1's synthetic
//     depth, one frequency up. The A-vs-B gap growing with size is the robust
//     finding; the absolute level depends on the documented depth assumptions.
//
// "Size regime" scales the gross notional: target_shares_i = w_i * base_notional
// * size_mult / close_i. A dollar-neutral book (sum w = 0) is held long/short;
// bigger size => bigger orders => more of the book walked in depth mode.
//
// Output per (mode, size): <out>/<name>/{naive,depth}_<size>_{equity,tradelog}.csv
// Analyzed by scripts/analysis/statarb_audit.py.

#include "qse/data/OrderBookFullDepth.h"
#include "qse/order/OrderManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kInitialCash = 1e9; // >> any gross notional, so the BUY cash
                                     // guard never distorts a dollar-neutral book

struct PxVol {
    double close = 0.0;
    double volume = 0.0;
};
using DayPrices = std::map<std::string, PxVol>;      // symbol -> px/vol
using PriceTable = std::map<std::string, DayPrices>; // date(YYYY-MM-DD, sorts chronologically)

struct Params {
    double base_notional = 1'000'000.0; // gross $ at size 1x
    std::size_t levels = 20;            // depth levels seeded per side
    double depth_frac = 0.05;           // fraction of scaled ADV visible in the book
    double adv_scale = 30.0;            // IEX-partial volume -> consolidated ADV approx
    double half_spread_bps = 1.0;       // synthetic half-spread (bps of price)
    double grid_bps = 2.0;              // price spacing between depth levels (bps)
};

// Days since the Unix epoch for a civil date (Howard Hinnant's algorithm),
// so timestamps are timezone-independent.
int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

int64_t date_to_ms(const std::string& ymd) {
    int y = 0, m = 0, d = 0;
    std::sscanf(ymd.c_str(), "%d-%d-%d", &y, &m, &d);
    return days_from_civil(y, static_cast<unsigned>(m), static_cast<unsigned>(d)) * 86'400'000LL;
}

std::string strip_dashes(const std::string& ymd) {
    std::string out;
    for (char c : ymd) {
        if (c != '-')
            out.push_back(c);
    }
    return out;
}

PriceTable load_prices(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open prices file: " + path);
    }
    PriceTable table;
    std::string line;
    std::getline(in, line); // header: date,symbol,close,volume
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string date, sym, close, vol;
        if (!std::getline(ss, date, ',') || !std::getline(ss, sym, ',') ||
            !std::getline(ss, close, ',') || !std::getline(ss, vol, ',')) {
            continue;
        }
        try {
            PxVol pv{std::stod(close), std::stod(vol)};
            if (pv.close > 0.0) {
                table[date][sym] = pv;
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    return table;
}

// weights_YYYYMMDD.csv (symbol,weight). Empty map => no file for that date
// (hold current positions rather than force flat).
std::map<std::string, double> load_weights(const std::string& dir, const std::string& ymd) {
    std::map<std::string, double> weights;
    std::string path = dir + "/weights_" + strip_dashes(ymd) + ".csv";
    std::ifstream in(path);
    if (!in.is_open()) {
        return weights;
    }
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string sym, w;
        if (!std::getline(ss, sym, ',') || !std::getline(ss, w, ','))
            continue;
        try {
            weights[sym] = std::stod(w);
        } catch (const std::exception&) {
            continue;
        }
    }
    return weights;
}

struct Seed {
    qse::Order::Side side;
    qse::Price price;
    qse::QueueId qid;
};

void run_one(const std::string& name, bool depth_mode, double size_mult, const PriceTable& prices,
             const std::string& weights_dir, const Params& p, const std::string& out_prefix) {
    qse::OrderManager om(kInitialCash, out_prefix + "_equity.csv", out_prefix + "_tradelog.csv");
    om.set_use_full_depth(depth_mode);

    std::map<std::string, std::vector<Seed>> seeds; // per-symbol seeded liquidity
    long long rebalances = 0;

    for (const auto& [date, day] : prices) {
        auto weights = load_weights(weights_dir, date);
        const bool have_targets = !weights.empty();

        for (const auto& [sym, pv] : day) {
            const double half_spread = pv.close * p.half_spread_bps / 1e4;
            const double grid = std::max(0.01, pv.close * p.grid_bps / 1e4);

            qse::Tick tick;
            tick.symbol = sym;
            tick.timestamp = qse::Timestamp(std::chrono::milliseconds(date_to_ms(date)));
            tick.price = pv.close;
            tick.bid = pv.close - half_spread;
            tick.ask = pv.close + half_spread;
            // naive: infinite liquidity at the mid; depth: nominal trade print
            // so consume_trade_print doesn't eat the book before we walk it
            tick.volume = depth_mode ? 1 : 1'000'000'000'000LL;
            tick.bid_size = tick.volume;
            tick.ask_size = tick.volume;

            if (depth_mode) {
                auto& book = om.depth_book(sym);
                for (const Seed& s : seeds[sym]) {
                    book.cancel_order(s.qid);
                    book.remove_level_if_empty(s.side, s.price);
                }
                seeds[sym].clear();
                const auto per_level = static_cast<qse::Volume>(std::max(
                    1.0, p.adv_scale * pv.volume * p.depth_frac / static_cast<double>(p.levels)));
                for (std::size_t lvl = 1; lvl <= p.levels; ++lvl) {
                    const qse::Price ask_px = tick.ask + static_cast<double>(lvl) * grid;
                    const qse::Price bid_px = tick.bid - static_cast<double>(lvl) * grid;
                    const qse::QueueId aq = book.enqueue_order(
                        qse::Order::Side::SELL, ask_px, "seed_a" + std::to_string(lvl), per_level);
                    const qse::QueueId bq = book.enqueue_order(
                        qse::Order::Side::BUY, bid_px, "seed_b" + std::to_string(lvl), per_level);
                    seeds[sym].push_back({qse::Order::Side::SELL, ask_px, aq});
                    seeds[sym].push_back({qse::Order::Side::BUY, bid_px, bq});
                }
            }

            if (have_targets) {
                const double w = weights.count(sym) ? weights[sym] : 0.0;
                const long long target = std::llround(w * p.base_notional * size_mult / pv.close);
                const long long current = om.get_position(sym);
                const long long delta = target - current;
                if (delta > 0) {
                    om.submit_market_order(sym, qse::Order::Side::BUY,
                                           static_cast<qse::Volume>(delta));
                    ++rebalances;
                } else if (delta < 0) {
                    om.submit_market_order(sym, qse::Order::Side::SELL,
                                           static_cast<qse::Volume>(-delta));
                    ++rebalances;
                }
            }

            om.process_tick(tick);
            om.attempt_fills();
        }

        std::map<std::string, double> closes;
        for (const auto& [sym, pv] : day) {
            closes[sym] = pv.close;
        }
        om.record_equity(date_to_ms(date), closes);
    }

    double final_equity = om.get_cash();
    const auto& last_day = prices.rbegin()->second;
    for (const auto& [sym, pv] : last_day) {
        final_equity += om.get_position(sym) * pv.close;
    }
    std::cout << name << " " << (depth_mode ? "depth" : "naive") << " size " << size_mult
              << "x: " << rebalances << " rebalance orders, final equity " << std::fixed
              << final_equity << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string prices_path = "data/universe/prices.csv";
    std::string weights_dir = "data/universe/weights";
    std::string name = "stat_arb";
    std::string out_dir = "results/statarb_audit";
    std::vector<double> sizes = {1.0, 10.0, 50.0};
    Params p;

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        std::string value = argv[i + 1];
        if (flag == "--prices")
            prices_path = value;
        else if (flag == "--weights-dir")
            weights_dir = value;
        else if (flag == "--name")
            name = value;
        else if (flag == "--out-dir")
            out_dir = value;
        else if (flag == "--base-notional")
            p.base_notional = std::stod(value);
        else if (flag == "--depth-frac")
            p.depth_frac = std::stod(value);
        else if (flag == "--adv-scale")
            p.adv_scale = std::stod(value);
        else if (flag == "--levels")
            p.levels = static_cast<std::size_t>(std::stoul(value));
        else if (flag == "--sizes") {
            sizes.clear();
            std::stringstream ss(value);
            std::string tok;
            while (std::getline(ss, tok, ','))
                sizes.push_back(std::stod(tok));
        } else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    PriceTable prices;
    try {
        prices = load_prices(prices_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    if (prices.empty()) {
        std::cerr << "No prices loaded from " << prices_path << "\n";
        return 1;
    }

    const std::string run_dir = out_dir + "/" + name;
    std::filesystem::create_directories(run_dir);

    for (double size : sizes) {
        for (bool depth_mode : {false, true}) {
            std::ostringstream sz;
            sz << (depth_mode ? "depth" : "naive") << "_" << static_cast<long long>(size);
            run_one(name, depth_mode, size, prices, weights_dir, p, run_dir + "/" + sz.str());
        }
    }

    std::cout << "Stat-arb audit for '" << name << "' written to " << run_dir << "\n";
    return 0;
}
