#include "BarBuilder.h"
#include <algorithm> // for std::max/min

namespace qse {

BarBuilder::BarBuilder(const std::chrono::seconds& bar_interval)
    : bar_interval_(bar_interval) {}

std::optional<Bar> BarBuilder::add_tick(const Tick& tick) {
    // If this is the very first tick we've ever seen, start the first bar.
    if (!current_bar_.has_value()) {
        start_new_bar(tick);
        return std::nullopt; // No bar is complete yet.
    }

    // Check if the new tick belongs to the next bar interval.
    if (tick.timestamp >= current_bar_start_time_ + bar_interval_) {
        // The current bar is now complete.
        Bar completed_bar = *current_bar_;
        
        // Start a new bar with the current tick.
        start_new_bar(tick);

        // Return the completed bar.
        return completed_bar;
    } else {
        // The tick is within the current bar's time interval. Update the high, low, close, and volume.
        current_bar_->high = std::max(current_bar_->high, tick.price);
        current_bar_->low = std::min(current_bar_->low, tick.price);
        current_bar_->close = tick.price;
        current_bar_->volume += tick.volume;
        return std::nullopt; // The bar is not yet complete.
    }
}

void BarBuilder::start_new_bar(const Tick& tick) {
    // Calculate the start time of the new bar by rounding the tick's timestamp down
    // to the nearest interval. For example, a tick at 00:01:23 with a 60s interval
    // will start a new bar at 00:01:00.
    auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        tick.timestamp.time_since_epoch()
    );
    long long interval_seconds = bar_interval_.count();
    long long rounded_seconds = (seconds_since_epoch.count() / interval_seconds) * interval_seconds;
    
    current_bar_start_time_ = Timestamp(std::chrono::seconds(rounded_seconds));

    // Initialize the new bar's OHLCV data with the tick's data.
    current_bar_.emplace(Bar{
        .timestamp = current_bar_start_time_,
        .open = tick.price,
        .high = tick.price,
        .low = tick.price,
        .close = tick.price,
        .volume = tick.volume
    });
}

} // namespace qse