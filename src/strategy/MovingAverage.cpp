#include "qse/strategy/MovingAverage.h"

namespace qse {

MovingAverage::MovingAverage(size_t window_size) : window_size_(window_size) {}

void MovingAverage::update(double price) {
    prices_.push_back(price);
    current_sum_ += price;

    if (prices_.size() > window_size_) {
        // O(1) optimization: subtract the oldest element
        current_sum_ -= prices_.front();
        prices_.pop_front();
    }
}

double MovingAverage::get_value() const {
    if (!is_ready()) {
        return 0.0;
    }
    return current_sum_ / window_size_;
}

bool MovingAverage::is_ready() const {
    return prices_.size() == window_size_;
}

} // namespace qse 