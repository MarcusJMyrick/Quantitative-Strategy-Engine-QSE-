#pragma once

#include <deque>
#include <numeric>

namespace qse {

class MovingAverage {
public:
    explicit MovingAverage(size_t window_size);

    void update(double price);
    double get_value() const;
    bool is_ready() const;

private:
    const size_t window_size_;
    std::deque<double> prices_;
    double current_sum_ = 0.0;
};

} // namespace qse 