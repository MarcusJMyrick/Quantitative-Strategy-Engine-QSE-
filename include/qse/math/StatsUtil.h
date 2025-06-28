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

    std::size_t count() const { return buf_.size(); }

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

/// ----------------------------------------------
/// Rolling (windowed) covariance & variance helper
/// ----------------------------------------------
class RollingCovariance {
public:
    explicit RollingCovariance(std::size_t window) : window_(window) {}

    // Feed paired observations (x, y); returns cov of current window
    double operator()(double x, double y) {
        buf_x_.push_back(x);
        buf_y_.push_back(y);
        sum_x_  += x;
        sum_y_  += y;
        sum_xy_ += x * y;

        if (buf_x_.size() > window_) {
            double old_x = buf_x_.front(); buf_x_.pop_front();
            double old_y = buf_y_.front(); buf_y_.pop_front();
            sum_x_  -= old_x;
            sum_y_  -= old_y;
            sum_xy_ -= old_x * old_y;
        }

        auto n = static_cast<double>(buf_x_.size());
        if (n < 2) return 0.0;
        double mean_x = sum_x_ / n;
        double mean_y = sum_y_ / n;
        return (sum_xy_ / n) - mean_x * mean_y;
    }

    std::size_t count() const { return buf_x_.size(); }

private:
    std::size_t      window_;
    std::deque<double> buf_x_;
    std::deque<double> buf_y_;
    double           sum_x_{0.0}, sum_y_{0.0}, sum_xy_{0.0};
};

class RollingVariance {
public:
    explicit RollingVariance(std::size_t window) : window_(window) {}

    double operator()(double x) {
        buf_.push_back(x);
        sum_  += x;
        sum2_ += x * x;
        if (buf_.size() > window_) {
            double old = buf_.front();
            buf_.pop_front();
            sum_  -= old;
            sum2_ -= old * old;
        }
        auto n = static_cast<double>(buf_.size());
        if (n < 2) return 0.0;
        double mean = sum_ / n;
        return (sum2_ / n) - mean * mean;
    }

    std::size_t count() const { return buf_.size(); }

private:
    std::size_t      window_;
    std::deque<double> buf_;
    double           sum_{0.0};
    double           sum2_{0.0};
};

}   // namespace qse::math 