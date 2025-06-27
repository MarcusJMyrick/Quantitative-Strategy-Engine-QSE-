#pragma once
#include <deque>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

namespace qse::math {

/// ----------------------------------------------
/// Rolling (windowed) population standard deviation
/// ----------------------------------------------
class RollingStdDev {
public:
    explicit RollingStdDev(std::size_t window) : window_(window) {}

    /// Feed one new observation; returns σ of *current* window.
    double operator()(double x) {
        buf_.push_back(x);
        sum_  += x;
        sum2_ += x * x;

        if (buf_.size() > window_) {
            auto old = buf_.front();
            buf_.pop_front();
            sum_  -= old;
            sum2_ -= old * old;
        }

        auto n = static_cast<double>(buf_.size());
        if (n < 2) return 0.0;
        double mean = sum_ / n;
        return std::sqrt(std::max(0.0, (sum2_ / n) - mean * mean));
    }

private:
    std::size_t      window_;
    std::deque<double> buf_;
    double           sum_{0.0};
    double           sum2_{0.0};
};

/// Winsorise in-place – clamp values to the q-th / (1-q) quantile.
inline void winsorize(std::vector<double>& v, double q = 0.01) {
    if (v.empty()) return;
    auto w = v;                     // copy for quantile lookup
    std::nth_element(w.begin(), w.begin() + w.size() * q, w.end());
    double lo = w[static_cast<std::size_t>(w.size() * q)];
    std::nth_element(w.begin(), w.begin() + w.size() * (1 - q), w.end());
    double hi = w[static_cast<std::size_t>(w.size() * (1 - q))];

    for (auto& x : v) x = std::clamp(x, lo, hi);
}

/// Simple z-score (vector is modified in-place)
inline void zscore(std::vector<double>& v) {
    if (v.empty()) return;
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    double var  = 0.0;
    for (auto x : v) var += (x - mean) * (x - mean);
    double sd = std::sqrt(var / v.size());
    if (sd == 0.0) sd = 1.0;
    for (auto& x : v) x = (x - mean) / sd;
}

}   // namespace qse::math 