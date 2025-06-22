#pragma once

#include <deque>
#include <cmath>

namespace qse {

    /**
     * @class MovingStandardDeviation
     * @brief Calculates the moving standard deviation of a series of values.
     *
     * This class maintains a rolling window of values and calculates the standard deviation
     * of the values in that window. It's useful for statistical analysis in trading strategies.
     */
    class MovingStandardDeviation {
    public:
        /**
         * @brief Constructor.
         * @param window_size The size of the rolling window for calculating standard deviation.
         */
        explicit MovingStandardDeviation(int window_size);

        /**
         * @brief Add a new value to the series and update the standard deviation.
         * @param value The new value to add.
         */
        void update(double value);

        /**
         * @brief Get the current standard deviation.
         * @return The standard deviation of the values in the current window.
         */
        double get_value() const;

        /**
         * @brief Check if the standard deviation calculator has enough data.
         * @return True if the window is full and standard deviation can be calculated.
         */
        bool is_warmed_up() const;

        /**
         * @brief Reset the calculator to its initial state.
         */
        void reset();

    private:
        int window_size_;
        std::deque<double> values_;
        double sum_;
        double sum_squares_;
    };

} // namespace qse 