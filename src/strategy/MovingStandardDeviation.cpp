#include "qse/strategy/MovingStandardDeviation.h"
#include <cmath>

namespace qse {

    MovingStandardDeviation::MovingStandardDeviation(int window_size)
        : window_size_(window_size), sum_(0.0), sum_squares_(0.0) {
        if (window_size <= 0) {
            throw std::invalid_argument("Window size must be positive");
        }
    }

    void MovingStandardDeviation::update(double value) {
        // Add the new value
        values_.push_back(value);
        sum_ += value;
        sum_squares_ += value * value;

        // Remove the oldest value if the window is full
        if (values_.size() > static_cast<size_t>(window_size_)) {
            double old_value = values_.front();
            values_.pop_front();
            sum_ -= old_value;
            sum_squares_ -= old_value * old_value;
        }
    }

    double MovingStandardDeviation::get_value() const {
        if (!is_warmed_up()) {
            return 0.0;
        }

        int n = static_cast<int>(values_.size());
        double mean = sum_ / n;
        double variance = (sum_squares_ / n) - (mean * mean);
        
        // Ensure variance is non-negative (handles floating point precision issues)
        if (variance < 0.0) {
            variance = 0.0;
        }
        
        return std::sqrt(variance);
    }

    bool MovingStandardDeviation::is_warmed_up() const {
        return values_.size() >= static_cast<size_t>(window_size_);
    }

    void MovingStandardDeviation::reset() {
        values_.clear();
        sum_ = 0.0;
        sum_squares_ = 0.0;
    }

} // namespace qse 