#pragma once

#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>

#include "qse/data/Data.h"

namespace qse {

/**
 * @class VPINCalculator
 * @brief Volume-synchronized Probability of Informed Trading (Easley-López de
 *        Prado-O'Hara 2012).
 *
 * QR1.2. Toxicity of order flow, measured in *volume* time rather than clock
 * time:
 *   1. Equal-volume buckets — trades are accumulated into buckets of exactly
 *      `bucket_volume` shares (a large trade is split across buckets).
 *   2. Bulk-volume classification — the price change between consecutive bucket
 *      closes is standardized and pushed through the normal CDF to split each
 *      bucket's volume into buy vs sell: V_buy = V·Φ(ΔP/σ), V_sell = V − V_buy.
 *   3. VPIN = mean over the last `num_buckets` buckets of |V_buy − V_sell| / V
 *      = mean of |2·Φ(ΔP/σ) − 1| (the per-bucket order imbalance).
 *
 * σ is the volatility of the bucketed price changes; by default it is estimated
 * causally as the expanding sample std of ΔP, or a `fixed_sigma` may be supplied
 * (deterministic — used to test a known buy/sell split). Balanced flow (ΔP ≈ 0)
 * gives VPIN ≈ 0; one-sided flow (large |ΔP|/σ) gives VPIN ≈ 1.
 *
 * Scope (docs/thesis/limitations.md §1): a toxicity / execution-timing signal on
 * L1-reconstructed flow, not a price-prediction alpha. Feeds the OrderManager
 * toxicity filter in QR1.3.
 */
class VPINCalculator {
public:
    VPINCalculator(Volume bucket_volume, std::size_t num_buckets, double fixed_sigma = 0.0)
        : bucket_volume_(bucket_volume), num_buckets_(num_buckets), fixed_sigma_(fixed_sigma) {}

    /// Standard normal CDF Φ (erfc form for tail accuracy).
    static double normal_cdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    /// Buy fraction Φ(ΔP/σ); balanced (0.5) when σ is non-positive (no info).
    static double buy_fraction(double delta_p, double sigma) {
        if (sigma <= 0.0) {
            return 0.5;
        }
        return normal_cdf(delta_p / sigma);
    }

    /// Per-bucket order imbalance |V_buy − V_sell| / V = |2·Φ(ΔP/σ) − 1|.
    static double order_imbalance(double delta_p, double sigma) {
        return std::abs(2.0 * buy_fraction(delta_p, sigma) - 1.0);
    }

    /// Feed one trade (price, volume); fills/splits equal-volume buckets.
    void on_trade(Price price, Volume volume) {
        Volume remaining = volume;
        while (remaining > 0) {
            const Volume take = std::min(remaining, bucket_volume_ - partial_volume_);
            partial_volume_ += take;
            remaining -= take;
            partial_close_ = price; // bucket's close is its last trade price
            if (partial_volume_ == bucket_volume_) {
                complete_bucket(partial_close_);
                partial_volume_ = 0;
            }
        }
    }

    bool ready() const { return imbalances_.size() >= num_buckets_; }

    /// VPIN over the last `num_buckets` buckets; NaN until `ready()`.
    double vpin() const {
        if (!ready()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double sum = 0.0;
        for (double v : imbalances_) {
            sum += v;
        }
        return sum / static_cast<double>(imbalances_.size());
    }

    std::size_t total_buckets() const { return total_buckets_; }
    double current_sigma() const { return sigma_estimate(); }

    void reset() {
        imbalances_.clear();
        partial_volume_ = 0;
        total_buckets_ = 0;
        have_prev_close_ = false;
        n_delta_ = 0;
        sum_delta_ = 0.0;
        sumsq_delta_ = 0.0;
    }

private:
    double sigma_estimate() const {
        if (fixed_sigma_ > 0.0) {
            return fixed_sigma_;
        }
        if (n_delta_ < 2) {
            return 0.0; // not enough ΔP yet → buy_fraction defaults to 0.5
        }
        const double mean = sum_delta_ / static_cast<double>(n_delta_);
        const double var = (sumsq_delta_ - static_cast<double>(n_delta_) * mean * mean) /
                           static_cast<double>(n_delta_ - 1);
        return var > 0.0 ? std::sqrt(var) : 0.0;
    }

    void complete_bucket(Price close) {
        ++total_buckets_;
        if (have_prev_close_) {
            const double delta = close - prev_close_;
            ++n_delta_;
            sum_delta_ += delta;
            sumsq_delta_ += delta * delta;
            imbalances_.push_back(order_imbalance(delta, sigma_estimate()));
            if (imbalances_.size() > num_buckets_) {
                imbalances_.pop_front();
            }
        }
        prev_close_ = close;
        have_prev_close_ = true;
    }

    Volume bucket_volume_;
    std::size_t num_buckets_;
    double fixed_sigma_;

    std::deque<double> imbalances_;
    Volume partial_volume_ = 0;
    Price partial_close_ = 0.0;
    std::size_t total_buckets_ = 0;

    bool have_prev_close_ = false;
    Price prev_close_ = 0.0;

    // expanding ΔP statistics for the causal σ estimate
    std::size_t n_delta_ = 0;
    double sum_delta_ = 0.0;
    double sumsq_delta_ = 0.0;
};

} // namespace qse
