#include "qse/data/BarBuilder.h"
#include <algorithm> // for std::max/min
#include <iostream>

#ifndef QSE_ENABLE_VERBOSE
// Disable all std::cout in this TU for performance
static struct DisableBarBuilderCout { DisableBarBuilderCout(){ std::cout.setstate(std::ios_base::failbit); } } _disable_bb_cout;
#endif

namespace qse {

BarBuilder::BarBuilder(const std::chrono::seconds& bar_interval)
    : bar_interval_(bar_interval)
{}

// Add a tick, then if any completed bars are queued, pop & return one.
std::optional<Bar> BarBuilder::add_tick(const Tick& tick) {
    std::cout << "Adding tick: timestamp=" << tick.timestamp.time_since_epoch().count() 
              << ", price=" << tick.price << ", volume=" << tick.volume << std::endl;
    
    // 1) buffer + sort
    tick_buffer_.push_back(tick);
    std::sort(tick_buffer_.begin(), tick_buffer_.end(),
              [](auto &a, auto &b){ return a.timestamp < b.timestamp; });

    std::cout << "Buffer size after sort: " << tick_buffer_.size() << std::endl;

    // 2) process everything
    process_buffered_ticks();

    std::cout << "Ready bars queue size: " << ready_bars_.size() << std::endl;

    // 3) if there's a ready bar, return the oldest
    if (!ready_bars_.empty()) {
        Bar b = ready_bars_.front();
        ready_bars_.pop_front();
        std::cout << "Returning bar: timestamp=" << b.timestamp.time_since_epoch().count() 
                  << ", open=" << b.open << ", high=" << b.high 
                  << ", low=" << b.low << ", close=" << b.close 
                  << ", volume=" << b.volume << std::endl;
        return b;
    }
    return std::nullopt;
}

// Flush any remaining: first finish processing ticks, then
// return any bars left (one per call).
std::optional<Bar> BarBuilder::flush() {
    std::cout << "Flush called, ready bars: " << ready_bars_.size() << std::endl;
    process_buffered_ticks();
    if (!ready_bars_.empty()) {
        Bar b = ready_bars_.front();
        ready_bars_.pop_front();
        std::cout << "Flush returning bar: timestamp=" << b.timestamp.time_since_epoch().count() 
                  << ", open=" << b.open << ", high=" << b.high 
                  << ", low=" << b.low << ", close=" << b.close 
                  << ", volume=" << b.volume << std::endl;
        return b;
    }
    // finally, if there's an in‐progress bar left, emit it and clear
    if (current_bar_) {
        Bar b = *current_bar_;
        current_bar_.reset();
        std::cout << "Flush returning current bar: timestamp=" << b.timestamp.time_since_epoch().count() 
                  << ", open=" << b.open << ", high=" << b.high 
                  << ", low=" << b.low << ", close=" << b.close 
                  << ", volume=" << b.volume << std::endl;
        return b;
    }
    return std::nullopt;
}

// Internal: walk through all buffered ticks, emitting bars on each boundary.
void BarBuilder::process_buffered_ticks() {
    std::cout << "Processing " << tick_buffer_.size() << " buffered ticks" << std::endl;
    
    while (!tick_buffer_.empty()) {
        const auto tick = tick_buffer_.front();
        tick_buffer_.erase(tick_buffer_.begin());

        std::cout << "Processing tick: timestamp=" << tick.timestamp.time_since_epoch().count() 
                  << ", price=" << tick.price << std::endl;

        // first-ever tick: start first bar
        if (!current_bar_) {
            std::cout << "Starting first bar" << std::endl;
            start_new_bar(tick);
            continue;
        }

        // if tick falls beyond current interval, possibly multiple intervals:
        auto bucket_end = current_bar_start_time_ + bar_interval_;
        std::cout << "Current bar start: " << current_bar_start_time_.time_since_epoch().count() 
                  << ", bucket end: " << bucket_end.time_since_epoch().count() 
                  << ", tick timestamp: " << tick.timestamp.time_since_epoch().count() << std::endl;
        
        if (tick.timestamp >= bucket_end) {
            // emit current
            std::cout << "Emitting current bar to ready queue" << std::endl;
            ready_bars_.push_back(*current_bar_);

            // advance start by exactly one interval at a time
            // until the tick fits into [start, start+interval)
            do {
                current_bar_start_time_ += bar_interval_;
                std::cout << "Advanced bar start to: " << current_bar_start_time_.time_since_epoch().count() << std::endl;
            } while (tick.timestamp >= current_bar_start_time_ + bar_interval_);

            // start a new bar at that aligned start, initialised with this tick
            std::cout << "Starting new bar" << std::endl;
            start_new_bar(tick);
        } else if (tick.timestamp < current_bar_start_time_) {
            // tick is before current bar start: emit current bar and start new one
            std::cout << "Tick before current bar start, emitting current bar" << std::endl;
            ready_bars_.push_back(*current_bar_);
            
            // start a new bar at the correct time bucket for this tick
            std::cout << "Starting new bar for earlier tick" << std::endl;
            start_new_bar(tick);
        } else {
            // same interval: update OHLCV
            std::cout << "Updating current bar" << std::endl;
            current_bar_->high   = std::max(current_bar_->high,  tick.price);
            current_bar_->low    = std::min(current_bar_->low,   tick.price);
            current_bar_->close  = tick.price;
            current_bar_->volume += tick.volume;
        }
    }
}

// Align down to nearest interval and init current_bar_
void BarBuilder::start_new_bar(const Tick& tick) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        tick.timestamp.time_since_epoch());
    auto interval = bar_interval_.count();
    auto start_secs = (secs.count() / interval) * interval;
    current_bar_start_time_ = Timestamp(std::chrono::seconds(start_secs));

    std::cout << "Starting new bar at: " << current_bar_start_time_.time_since_epoch().count() 
              << " for tick at: " << tick.timestamp.time_since_epoch().count() << std::endl;

    current_bar_.emplace( Bar{
      .symbol    = tick.symbol,
      .timestamp = current_bar_start_time_,
      .open      = tick.price,
      .high      = tick.price,
      .low       = tick.price,
      .close     = tick.price,
      .volume    = tick.volume
    } );
}

} // namespace qse