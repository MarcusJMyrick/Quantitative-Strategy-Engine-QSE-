// Impact sweep tool (roadmap A4): sweeps market-order sizes through
// OrderBookFullDepth against synthetic depth reconstructed behind real tick
// prices, and writes realized slippage per (profile, sample, size).
//
// The tick file carries trades only (timestamp,price,volume), so the quote is
// synthesized around each sampled trade price and depth behind the touch
// follows a parametric profile:
//   uniform - every level has base_size shares  (theory: impact ~ Q^1)
//   linear  - level i has base_size*(i+1) shares (theory: impact ~ Q^0.5)
//
// Output CSV: profile,sample,mid,size,filled,vwap,slip_bps_vs_touch,slip_bps_vs_mid
// Analyzed by scripts/analysis/impact_study.py.

#include "qse/data/OrderBookFullDepth.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct TradeTick {
    double price = 0.0;
};

std::vector<TradeTick> load_trade_prices(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open tick file: " + path);
    }
    std::vector<TradeTick> ticks;
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string ts, price_str;
        if (!std::getline(ss, ts, ',') || !std::getline(ss, price_str, ',')) {
            continue;
        }
        try {
            TradeTick t;
            t.price = std::stod(price_str);
            if (t.price > 0.0) {
                ticks.push_back(t);
            }
        } catch (const std::exception&) {
            // Skip malformed rows
        }
    }
    return ticks;
}

} // namespace

int main(int argc, char** argv) {
    std::string ticks_path = "data/raw_ticks_AAPL.csv";
    std::string out_path = "results/impact_sweep.csv";
    std::size_t n_samples = 200;
    std::size_t n_levels = 800;

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        std::string value = argv[i + 1];
        if (flag == "--ticks") ticks_path = value;
        else if (flag == "--out") out_path = value;
        else if (flag == "--samples") n_samples = std::stoul(value);
        else if (flag == "--levels") n_levels = std::stoul(value);
        else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    const double tick_size = 0.01;      // price grid behind the touch
    const qse::Volume base_size = 100;  // shares at the touch level
    const std::vector<qse::Volume> order_sizes = {
        50, 100, 200, 400, 800, 1600, 3200, 6400, 12800, 25600, 51200};
    const std::vector<std::string> profiles = {"uniform", "linear"};

    std::vector<TradeTick> ticks;
    try {
        ticks = load_trade_prices(ticks_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    if (ticks.empty()) {
        std::cerr << "No usable ticks in " << ticks_path << "\n";
        return 1;
    }
    if (n_samples > ticks.size()) {
        n_samples = ticks.size();
    }

    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Could not open output file: " << out_path << "\n";
        return 1;
    }
    out << "profile,sample,mid,size,filled,vwap,slip_bps_vs_touch,slip_bps_vs_mid\n";

    const std::size_t stride = ticks.size() / n_samples;
    std::size_t rows = 0;

    for (std::size_t s = 0; s < n_samples; ++s) {
        const double mid = ticks[s * stride].price;
        const double best_ask = mid + tick_size; // synthetic half-spread of one tick

        for (const auto& profile : profiles) {
            for (qse::Volume q : order_sizes) {
                // Fresh book per experiment so every sweep sees full depth
                qse::OrderBookFullDepth book;
                for (std::size_t lvl = 0; lvl < n_levels; ++lvl) {
                    qse::Volume size = (profile == "linear")
                                           ? base_size * static_cast<qse::Volume>(lvl + 1)
                                           : base_size;
                    book.enqueue_order(qse::Order::Side::SELL,
                                       best_ask + static_cast<double>(lvl) * tick_size,
                                       "lvl_" + std::to_string(lvl), size);
                }

                auto result = book.fill_market(qse::Order::Side::BUY,
                                               static_cast<std::int64_t>(q));
                const qse::Volume filled = result.first;
                const double vwap = result.second;
                if (filled == 0) {
                    continue;
                }
                const double slip_touch = (vwap - best_ask) / mid * 1e4;
                const double slip_mid = (vwap - mid) / mid * 1e4;

                out << profile << ',' << s << ',' << mid << ',' << q << ','
                    << filled << ',' << vwap << ',' << slip_touch << ','
                    << slip_mid << '\n';
                ++rows;
            }
        }
    }

    std::cout << "Wrote " << rows << " sweep rows to " << out_path << "\n";
    return 0;
}
