#include "qse/factor/ICMonitor.h"
#include <arrow/table.h>
#include <arrow/array.h>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>

namespace qse {

double ICMonitor::spearman_rank_corr(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) return std::numeric_limits<double>::quiet_NaN();
    int n = x.size();
    std::vector<size_t> rx(n), ry(n);
    std::iota(rx.begin(), rx.end(), 0);
    std::iota(ry.begin(), ry.end(), 0);
    std::sort(rx.begin(), rx.end(), [&](size_t i, size_t j) { return x[i] < x[j]; });
    std::sort(ry.begin(), ry.end(), [&](size_t i, size_t j) { return y[i] < y[j]; });
    std::vector<double> rank_x(n), rank_y(n);
    for (int i = 0; i < n; ++i) rank_x[rx[i]] = i + 1;
    for (int i = 0; i < n; ++i) rank_y[ry[i]] = i + 1;
    double mean_x = (n + 1) / 2.0, mean_y = (n + 1) / 2.0;
    double num = 0, denom_x = 0, denom_y = 0;
    for (int i = 0; i < n; ++i) {
        double dx = rank_x[i] - mean_x, dy = rank_y[i] - mean_y;
        num += dx * dy;
        denom_x += dx * dx;
        denom_y += dy * dy;
    }
    if (denom_x == 0 || denom_y == 0) return std::numeric_limits<double>::quiet_NaN();
    return num / std::sqrt(denom_x * denom_y);
}

ICMonitor::ICResult ICMonitor::compute_ic(const std::shared_ptr<arrow::Table>& table,
                                          const std::string& factor_col,
                                          const std::string& return_col,
                                          const std::string& date_col,
                                          int window_size) {
    ICResult result;
    if (!table) return result;
    int n = table->num_rows();
    if (n == 0) return result;
    
    // Get chunked arrays for all columns
    std::shared_ptr<arrow::ChunkedArray> factor_chunked = table->GetColumnByName(factor_col);
    std::shared_ptr<arrow::ChunkedArray> return_chunked = table->GetColumnByName(return_col);
    std::shared_ptr<arrow::ChunkedArray> date_chunked = table->GetColumnByName(date_col);
    
    if (!factor_chunked || !return_chunked || !date_chunked) {
        return result;
    }
    
    // For simplicity, assume single chunk arrays
    if (factor_chunked->num_chunks() != 1 || return_chunked->num_chunks() != 1 || date_chunked->num_chunks() != 1) {
        return result;
    }
    
    auto factor_array = std::static_pointer_cast<arrow::DoubleArray>(factor_chunked->chunk(0));
    auto return_array = std::static_pointer_cast<arrow::DoubleArray>(return_chunked->chunk(0));
    auto date_array = std::static_pointer_cast<arrow::StringArray>(date_chunked->chunk(0));
    
    if (!factor_array || !return_array || !date_array) {
        return result;
    }
    
    // Group by date using a map to handle unsorted input
    std::map<std::string, std::vector<std::pair<double, double>>> date_groups;
    for (int i = 0; i < n; ++i) {
        if (!date_array->IsValid(i) || !factor_array->IsValid(i) || !return_array->IsValid(i)) {
            continue;
        }
        
        auto sv = date_array->GetView(i);
        std::string date(sv.data(), sv.size());
        double factor = factor_array->Value(i);
        double ret = return_array->Value(i);
        
        date_groups[date].push_back({factor, ret});
    }
    
    // Convert map to vectors for processing
    std::vector<std::string> unique_dates;
    for (const auto& [date, _] : date_groups) {
        unique_dates.push_back(date);
    }
    std::sort(unique_dates.begin(), unique_dates.end());
    std::vector<std::vector<double>> factors_by_date, returns_by_date;
    for (const auto& date : unique_dates) {
        const auto& pairs = date_groups[date];
        factors_by_date.emplace_back();
        returns_by_date.emplace_back();
        for (const auto& [factor, ret] : pairs) {
            factors_by_date.back().push_back(factor);
            returns_by_date.back().push_back(ret);
        }
    }
    
    // Compute daily IC
    for (size_t d = 0; d < unique_dates.size(); ++d) {
        if (factors_by_date[d].size() < 3) {
            result.daily_ic.push_back(std::numeric_limits<double>::quiet_NaN());
        } else {
            double ic = spearman_rank_corr(factors_by_date[d], returns_by_date[d]);
            result.daily_ic.push_back(ic);
        }
    }
    
    // Compute rolling mean/std
    int m = result.daily_ic.size();
    result.rolling_mean.resize(m);
    result.rolling_std.resize(m);
    for (int i = 0; i < m; ++i) {
        int start = std::max(0, i - window_size + 1);
        double sum = 0, sum2 = 0;
        int count = 0;
        for (int j = start; j <= i; ++j) {
            if (!std::isnan(result.daily_ic[j])) {
                sum += result.daily_ic[j];
                sum2 += result.daily_ic[j] * result.daily_ic[j];
                ++count;
            }
        }
        if (count == 0) {
            result.rolling_mean[i] = std::numeric_limits<double>::quiet_NaN();
            result.rolling_std[i] = std::numeric_limits<double>::quiet_NaN();
        } else {
            result.rolling_mean[i] = sum / count;
            double mean = result.rolling_mean[i];
            result.rolling_std[i] = std::sqrt(std::max(0.0, sum2 / count - mean * mean));
        }
    }
    
    return result;
}

} // namespace qse 