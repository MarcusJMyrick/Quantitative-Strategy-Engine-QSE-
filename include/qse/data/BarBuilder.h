#pragma once

#include "qse/data/Data.h"
#include <optional>
#include <chrono>
#include <deque>
#include <vector>

namespace qse {

static_assert(sizeof(Timestamp) > 0, "Timestamp type not defined");
static_assert(sizeof(Tick) > 0, "Tick type not defined");
static_assert(sizeof(Bar) > 0, "Bar type not defined");

/**
 * @brief A utility class to construct time-based bars from a stream of ticks.
 * Handles out-of-order ticks by buffering and sorting them before processing.
 */
class BarBuilder {
public:
    /**
     * @brief Constructs a BarBuilder.
     * @param bar_interval The time duration for each bar (e.g., 60s for 1-minute bars).
     */
    explicit BarBuilder(const std::chrono::seconds& bar_interval = std::chrono::seconds(60));

    /**
     * @brief Feed a tick into the builder.  If one or more bars complete, returns
     *        the _oldest_ completed bar; otherwise std::nullopt.
     */
    std::optional<Bar> add_tick(const Tick& tick);

    /**
     * @brief Flush any remaining bars.  If there is any completed or in-progress
     *        bar left, returns it (one per call); otherwise std::nullopt.
     */
    std::optional<Bar> flush();

private:
    // interval and state
    std::chrono::seconds     bar_interval_;
    Timestamp           current_bar_start_time_;
    std::optional<Bar>  current_bar_;

    // buffers and ready queue
    std::vector<Tick>   tick_buffer_;
    std::deque<Bar>     ready_bars_;

    // internals
    void process_buffered_ticks();
    void start_new_bar(const Tick& tick);
};

} // namespace qse 