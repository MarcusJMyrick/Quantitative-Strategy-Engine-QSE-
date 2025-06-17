#pragma once

#include "Data.h"
#include <optional> // To indicate when a new bar is ready
#include <chrono>

namespace qse {

/**
 * @brief A utility class to construct time-based bars from a stream of ticks.
 */
class BarBuilder {
public:
    /**
     * @brief Constructs a BarBuilder.
     * @param bar_interval The time duration for each bar (e.g., 60s for 1-minute bars).
     */
    BarBuilder(const std::chrono::seconds& bar_interval = std::chrono::seconds(60));

    /**
     * @brief Processes a single tick. If a bar is completed with this tick, it is returned.
     * @param tick The incoming tick data.
     * @return An optional containing the completed Bar, or std::nullopt if the bar is not yet complete.
     */
    std::optional<Bar> add_tick(const Tick& tick);

private:
    // The time interval for each bar (e.g., 60 seconds for 1-minute bars).
    std::chrono::seconds bar_interval_;

    // The timestamp that marks the start of the current bar being built.
    Timestamp current_bar_start_time_;

    // The current bar being constructed.
    std::optional<Bar> current_bar_;

    // Helper to initialize a new bar based on the first tick of a new period.
    void start_new_bar(const Tick& tick);
};

} // namespace qse 