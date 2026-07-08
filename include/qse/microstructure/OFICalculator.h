#pragma once

#include <cstddef>
#include <deque>

#include "qse/data/Data.h"

namespace qse {

/**
 * @class OFICalculator
 * @brief Order Flow Imbalance (Cont-Kukanov-Stoikov) on L1 quotes.
 *
 * QR1.1. Per event (two consecutive L1 snapshots) the imbalance is
 * OFI = ΔV_bid − ΔV_ask, where each side's contribution is conditional on how
 * its price moved:
 *   bid up   → +new bid size        (demand stepped up)
 *   bid down → −prior bid size      (demand withdrawn)
 *   bid flat → new − prior size     (size delta at the same level)
 * and symmetrically on the ask (an ask that moves *up* withdraws supply, which
 * is bullish, so it adds the prior ask size; an ask that moves *down* adds new
 * supply, bearish, subtracting the new size). Positive OFI = net buying
 * pressure. Sizes are cast to double before differencing so the unsigned
 * Volume type never underflows.
 *
 * Scope (see docs/thesis/limitations.md §1): computed on L1-reconstructed
 * quotes, so this is an execution-timing / toxicity signal, not a price-
 * prediction alpha. The class keeps a rolling window sum for use as a live
 * OrderManager filter (QR1.3).
 */
class OFICalculator {
public:
    explicit OFICalculator(std::size_t window = 50) : window_(window) {}

    /// Pure per-event OFI (Cont et al.), exposed static for testing/reuse.
    static double event_ofi(Price prev_bid, Volume prev_bid_size, Price prev_ask,
                            Volume prev_ask_size, Price bid, Volume bid_size, Price ask,
                            Volume ask_size) {
        double bid_delta;
        if (bid > prev_bid) {
            bid_delta = static_cast<double>(bid_size); // demand stepped up
        } else if (bid < prev_bid) {
            bid_delta = -static_cast<double>(prev_bid_size); // demand withdrawn
        } else {
            bid_delta = static_cast<double>(bid_size) - static_cast<double>(prev_bid_size);
        }

        double ask_delta;
        if (ask > prev_ask) {
            ask_delta = static_cast<double>(prev_ask_size); // supply withdrawn → bullish
        } else if (ask < prev_ask) {
            ask_delta = -static_cast<double>(ask_size); // new supply → bearish
        } else {
            ask_delta = -(static_cast<double>(ask_size) - static_cast<double>(prev_ask_size));
        }

        return bid_delta + ask_delta;
    }

    /// Feed the next L1 snapshot; returns that event's OFI contribution
    /// (0 for the first snapshot, which has no predecessor).
    double update(const Tick& tick) {
        double e = 0.0;
        if (has_prev_) {
            e = event_ofi(prev_bid_, prev_bid_size_, prev_ask_, prev_ask_size_, tick.bid,
                          tick.bid_size, tick.ask, tick.ask_size);
            events_.push_back(e);
            rolling_sum_ += e;
            if (window_ > 0 && events_.size() > window_) {
                rolling_sum_ -= events_.front();
                events_.pop_front();
            }
        }
        prev_bid_ = tick.bid;
        prev_bid_size_ = tick.bid_size;
        prev_ask_ = tick.ask;
        prev_ask_size_ = tick.ask_size;
        has_prev_ = true;
        last_event_ = e;
        return e;
    }

    double last_event() const { return last_event_; }
    double rolling_ofi() const { return rolling_sum_; } ///< sum over the last `window` events
    std::size_t count() const { return events_.size(); }
    bool has_prev() const { return has_prev_; }

    void reset() {
        events_.clear();
        rolling_sum_ = 0.0;
        last_event_ = 0.0;
        has_prev_ = false;
    }

private:
    std::size_t window_;
    std::deque<double> events_;
    double rolling_sum_ = 0.0;
    double last_event_ = 0.0;
    bool has_prev_ = false;
    Price prev_bid_ = 0.0;
    Price prev_ask_ = 0.0;
    Volume prev_bid_size_ = 0;
    Volume prev_ask_size_ = 0;
};

} // namespace qse
