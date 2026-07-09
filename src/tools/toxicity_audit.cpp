// QR1.3 — does a VPIN+OFI toxicity filter reduce execution slippage vs blind
// market orders, on the same tick stream? The audit decides.
//
// A schedule of marketable orders (alternating buy/sell every K ticks) is
// executed two ways on the SAME AAPL tick stream:
//
//   BLIND    — every order crosses the spread immediately (pays the half-spread).
//   FILTERED — the ToxicityFilter (QR1.3) gates on the running VPIN + OFI: when
//              flow is toxic AND directionally favorable it rests passive at the
//              near touch and waits up to W ticks for a passive fill (capturing
//              the spread); otherwise it crosses now. If a rested order is not
//              filled within W ticks it falls back to crossing (adverse-drift
//              risk). Benign / adverse-direction orders cross like BLIND.
//
// Slippage per order is the signed cost vs the arrival mid (buy: fill − mid;
// sell: mid − fill), so lower is better and a passive fill is negative (price
// improvement). Ticks are trade prints; the L1 quote is reconstructed with a
// fixed half-spread (docs/thesis/limitations.md §1).
//
// Output: <out>/toxicity_orders.csv (per-order slippage for both policies),
// analyzed by scripts/analysis/toxicity_audit.py.

#include "qse/microstructure/OFICalculator.h"
#include "qse/microstructure/ToxicityFilter.h"
#include "qse/microstructure/VPINCalculator.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kHalfSpread = 0.01; // reconstructed L1 half-spread ($)

struct Row {
    double price = 0.0;
    long long volume = 0;
};

std::vector<Row> load_ticks(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open tick file: " + path);
    }
    std::vector<Row> rows;
    std::string line;
    std::getline(in, line); // header: timestamp,price,volume
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string ts, price, volume;
        if (!std::getline(ss, ts, ',') || !std::getline(ss, price, ',') ||
            !std::getline(ss, volume, ',')) {
            continue;
        }
        try {
            Row r{std::stod(price), std::stoll(volume)};
            if (r.price > 0.0 && r.volume > 0) {
                rows.push_back(r);
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    return rows;
}

} // namespace

int main(int argc, char** argv) {
    std::string ticks_path = "data/raw_ticks_AAPL.csv";
    std::string out_dir = "results/toxicity_audit";
    qse::Volume bucket_volume = 20'000; // ~2 ticks/bucket on AAPL 1-min data
    std::size_t num_buckets = 50;
    double vpin_threshold = 0.6;
    int order_every = 10; // schedule an order every N ticks
    int wait_ticks = 20;  // passive resting horizon

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i], value = argv[i + 1];
        if (flag == "--ticks")
            ticks_path = value;
        else if (flag == "--out-dir")
            out_dir = value;
        else if (flag == "--bucket-volume")
            bucket_volume = static_cast<qse::Volume>(std::stoull(value));
        else if (flag == "--num-buckets")
            num_buckets = static_cast<std::size_t>(std::stoul(value));
        else if (flag == "--threshold")
            vpin_threshold = std::stod(value);
        else if (flag == "--order-every")
            order_every = std::stoi(value);
        else if (flag == "--wait-ticks")
            wait_ticks = std::stoi(value);
        else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    std::vector<Row> ticks;
    try {
        ticks = load_ticks(ticks_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    if (static_cast<int>(ticks.size()) < wait_ticks + order_every) {
        std::cerr << "Not enough ticks in " << ticks_path << "\n";
        return 1;
    }

    qse::VPINCalculator vpin(bucket_volume, num_buckets);
    qse::OFICalculator ofi(/*window=*/num_buckets);
    qse::ToxicityFilter filter(vpin_threshold);

    std::filesystem::create_directories(out_dir);
    std::ofstream out(out_dir + "/toxicity_orders.csv");
    out << "tick,side,arrival_mid,vpin,ofi,rested,filled_passive,blind_slip,filtered_slip\n";

    long long n_orders = 0, n_rested = 0, n_passive_fills = 0;
    double blind_sum = 0.0, filtered_sum = 0.0;
    bool next_is_buy = true;

    for (std::size_t t = 0; t < ticks.size(); ++t) {
        const double price = ticks[t].price;
        // reconstructed L1 snapshot for the toxicity signals
        qse::Tick quote{};
        quote.symbol = "AAPL";
        quote.price = price;
        quote.bid = price - kHalfSpread;
        quote.ask = price + kHalfSpread;
        quote.bid_size = static_cast<qse::Volume>(ticks[t].volume);
        quote.ask_size = static_cast<qse::Volume>(ticks[t].volume);
        quote.volume = static_cast<qse::Volume>(ticks[t].volume);
        vpin.on_trade(price, quote.volume);
        ofi.update(quote);

        // schedule an order once the signals have warmed up and we have room to
        // look `wait_ticks` ahead
        if (!vpin.ready() || t % static_cast<std::size_t>(order_every) != 0 ||
            t + static_cast<std::size_t>(wait_ticks) >= ticks.size()) {
            continue;
        }

        const bool is_buy = next_is_buy;
        next_is_buy = !next_is_buy;
        const double mid = price;
        const double v = vpin.vpin();
        const double o = ofi.rolling_ofi();

        // BLIND: cross now, pay the half-spread
        const double blind_slip = kHalfSpread;

        // FILTERED
        double filtered_slip = kHalfSpread;
        bool rested = false, filled_passive = false;
        if (filter.decide(is_buy, v, o) == qse::ExecAction::RestPassive) {
            rested = true;
            const double passive = is_buy ? (mid - kHalfSpread) : (mid + kHalfSpread);
            for (int w = 1; w <= wait_ticks; ++w) {
                const double fut = ticks[t + static_cast<std::size_t>(w)].price;
                if ((is_buy && fut <= passive) || (!is_buy && fut >= passive)) {
                    filled_passive = true;
                    filtered_slip = is_buy ? (passive - mid) : (mid - passive); // = −half_spread
                    break;
                }
            }
            if (!filled_passive) {
                // fall back to crossing at the touch `wait_ticks` later
                const double fb = ticks[t + static_cast<std::size_t>(wait_ticks)].price;
                filtered_slip = is_buy ? (fb + kHalfSpread - mid) : (mid - (fb - kHalfSpread));
            }
        }

        out << t << "," << (is_buy ? "BUY" : "SELL") << "," << mid << "," << v << "," << o << ","
            << (rested ? 1 : 0) << "," << (filled_passive ? 1 : 0) << "," << blind_slip << ","
            << filtered_slip << "\n";
        ++n_orders;
        n_rested += rested;
        n_passive_fills += filled_passive;
        blind_sum += blind_slip;
        filtered_sum += filtered_slip;
    }

    std::cout << std::fixed;
    std::cout << "orders " << n_orders << ", rested " << n_rested << " (passive-filled "
              << n_passive_fills << ")\n";
    if (n_orders > 0) {
        std::cout << "avg slippage/order  blind " << blind_sum / static_cast<double>(n_orders)
                  << "  filtered " << filtered_sum / static_cast<double>(n_orders) << "\n";
    }
    std::cout << "Wrote " << out_dir << "/toxicity_orders.csv\n";
    return 0;
}
