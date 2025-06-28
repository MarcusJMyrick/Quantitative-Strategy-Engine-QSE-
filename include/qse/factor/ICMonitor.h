#pragma once
#include <vector>
#include <string>
#include <memory>

namespace arrow {
    class Table;
}

namespace qse {

/**
 * @class ICMonitor
 * @brief Computes Spearman rank information coefficient (IC) between factor and next-day return, with rolling stats
 *
 * 1. Computes daily Spearman rank IC between factor and next-day return
 * 2. Tracks 252-day rolling mean and std of IC
 */
class ICMonitor {
public:
    ICMonitor() = default;
    ~ICMonitor() = default;

    struct ICResult {
        std::vector<double> daily_ic;      // Daily IC values
        std::vector<double> rolling_mean;  // 252-day rolling mean
        std::vector<double> rolling_std;   // 252-day rolling std
    };

    /**
     * @brief Compute daily and rolling IC for a factor
     * @param table Arrow table with columns: date, factor, next_day_return
     * @param factor_col Name of the factor column
     * @param return_col Name of the next-day return column
     * @param date_col Name of the date column
     * @param window_size Rolling window size (default 252)
     * @return ICResult struct with daily and rolling IC stats
     */
    ICResult compute_ic(const std::shared_ptr<arrow::Table>& table,
                       const std::string& factor_col,
                       const std::string& return_col,
                       const std::string& date_col,
                       int window_size = 252);

private:
    double spearman_rank_corr(const std::vector<double>& x, const std::vector<double>& y);
};

} // namespace qse 