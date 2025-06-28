#include "qse/factor/CrossSectionalRegression.h"
#include "qse/math/StatsUtil.h"
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/array/array_primitive.h>
#include <arrow/array/array_binary.h>
#include <arrow/result.h>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace qse {

CrossSectionalRegression::RegressionResult 
CrossSectionalRegression::run_regression(const std::shared_ptr<arrow::Table>& factor_table,
                                        const std::string& date_column,
                                        const std::string& return_column,
                                        const std::vector<std::string>& factor_columns) {
    
    RegressionResult result;
    result.num_factors = factor_columns.size();
    
    // Prepare regression data
    auto [X, y] = prepare_regression_data(factor_table, return_column, factor_columns, 0, factor_table->num_rows());
    
    if (X.empty() || y.empty() || X[0].empty()) {
        result.num_observations = 0;
        return result;
    }
    
    result.num_observations = y.size();
    
    // Handle missing data and outliers
    handle_missing_data(X, y);
    winsorize_data(X, y);
    
    // Compute OLS estimates
    result.factor_returns = compute_ols_estimates(X, y);
    
    // Compute predicted values and residuals
    std::vector<double> y_pred(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        y_pred[i] = 0.0;
        for (size_t j = 0; j < result.factor_returns.size(); ++j) {
            y_pred[i] += X[j][i] * result.factor_returns[j];
        }
    }
    
    result.residuals = compute_residuals(X, y, result.factor_returns);
    
    // Compute standard errors
    result.factor_std_errors = compute_standard_errors(X, result.residuals, result.num_observations);
    
    // Compute t-statistics
    result.factor_t_stats = compute_t_statistics(result.factor_returns, result.factor_std_errors);
    
    // Compute R-squared
    result.total_r_squared = compute_r_squared(y, y_pred);
    
    // Compute individual factor R-squared
    result.factor_r_squared.resize(result.num_factors);
    for (size_t i = 0; i < result.num_factors; ++i) {
        std::vector<std::vector<double>> X_single = {X[i]};
        auto single_coeff = compute_ols_estimates(X_single, y);
        std::vector<double> y_pred_single(y.size());
        for (size_t j = 0; j < y.size(); ++j) {
            y_pred_single[j] = X[i][j] * single_coeff[0];
        }
        result.factor_r_squared[i] = compute_r_squared(y, y_pred_single);
    }
    
    return result;
}

std::vector<CrossSectionalRegression::RegressionResult> 
CrossSectionalRegression::run_rolling_regression(const std::shared_ptr<arrow::Table>& factor_table,
                                                const std::string& date_column,
                                                const std::string& return_column,
                                                const std::vector<std::string>& factor_columns,
                                                int window_size) {
    
    std::vector<RegressionResult> results;
    int total_rows = factor_table->num_rows();
    
    for (int start = 0; start <= total_rows - window_size; start += window_size) {
        int end = std::min(start + window_size, total_rows);
        
        auto [X, y] = prepare_regression_data(factor_table, return_column, factor_columns, start, end);
        
        if (X.empty() || y.empty() || X[0].empty()) {
            continue;
        }
        
        // Handle missing data and outliers
        handle_missing_data(X, y);
        winsorize_data(X, y);
        
        if (y.size() < 10) { // Minimum observations required
            continue;
        }
        
        RegressionResult result;
        result.num_factors = factor_columns.size();
        result.num_observations = y.size();
        
        // Compute OLS estimates
        result.factor_returns = compute_ols_estimates(X, y);
        result.residuals = compute_residuals(X, y, result.factor_returns);
        result.factor_std_errors = compute_standard_errors(X, result.residuals, result.num_observations);
        result.factor_t_stats = compute_t_statistics(result.factor_returns, result.factor_std_errors);
        
        // Compute predicted values and R-squared
        std::vector<double> y_pred(y.size());
        for (size_t i = 0; i < y.size(); ++i) {
            y_pred[i] = 0.0;
            for (size_t j = 0; j < result.factor_returns.size(); ++j) {
                y_pred[i] += X[j][i] * result.factor_returns[j];
            }
        }
        result.total_r_squared = compute_r_squared(y, y_pred);
        
        results.push_back(result);
    }
    
    return results;
}

CrossSectionalRegression::RiskDecomposition 
CrossSectionalRegression::compute_risk_decomposition(
    const std::vector<std::vector<double>>& factor_returns,
    const std::vector<std::vector<double>>& factor_exposures) {
    
    RiskDecomposition decomposition;
    
    if (factor_returns.empty() || factor_exposures.empty()) {
        return decomposition;
    }
    
    int num_factors = factor_returns.size();
    int num_periods = factor_returns[0].size();
    
    // Compute factor covariance matrix
    std::vector<std::vector<double>> factor_cov_matrix(num_factors, std::vector<double>(num_factors));
    
    for (int i = 0; i < num_factors; ++i) {
        for (int j = 0; j < num_factors; ++j) {
            double sum = 0.0;
            for (int t = 0; t < num_periods; ++t) {
                sum += factor_returns[i][t] * factor_returns[j][t];
            }
            factor_cov_matrix[i][j] = sum / (num_periods - 1);
        }
    }
    
    // Compute factor variances (diagonal of covariance matrix)
    decomposition.factor_variances.resize(num_factors);
    for (int i = 0; i < num_factors; ++i) {
        decomposition.factor_variances[i] = factor_cov_matrix[i][i];
    }
    
    // Compute factor covariances (off-diagonal elements)
    decomposition.factor_covariances.resize(num_factors * (num_factors - 1) / 2);
    int idx = 0;
    for (int i = 0; i < num_factors; ++i) {
        for (int j = i + 1; j < num_factors; ++j) {
            decomposition.factor_covariances[idx++] = factor_cov_matrix[i][j];
        }
    }
    
    // Compute portfolio variance
    decomposition.total_portfolio_variance = 0.0;
    for (int i = 0; i < num_factors; ++i) {
        for (int j = 0; j < num_factors; ++j) {
            if (i < factor_exposures.size() && j < factor_exposures.size()) {
                double exposure_i = factor_exposures[i].empty() ? 0.0 : factor_exposures[i][0];
                double exposure_j = factor_exposures[j].empty() ? 0.0 : factor_exposures[j][0];
                decomposition.total_portfolio_variance += exposure_i * exposure_j * factor_cov_matrix[i][j];
            }
        }
    }
    
    // Compute factor risk contributions
    decomposition.factor_contributions.resize(num_factors);
    for (int i = 0; i < num_factors; ++i) {
        double contribution = 0.0;
        for (int j = 0; j < num_factors; ++j) {
            if (i < factor_exposures.size() && j < factor_exposures.size()) {
                double exposure_i = factor_exposures[i].empty() ? 0.0 : factor_exposures[i][0];
                double exposure_j = factor_exposures[j].empty() ? 0.0 : factor_exposures[j][0];
                contribution += exposure_i * exposure_j * factor_cov_matrix[i][j];
            }
        }
        decomposition.factor_contributions[i] = contribution;
    }
    
    // Compute specific variance (residual variance)
    decomposition.specific_variance = 0.0;
    // This would typically be computed from regression residuals
    // For now, we'll use a simple estimate
    
    return decomposition;
}

std::string CrossSectionalRegression::generate_attribution_report(
    const std::vector<RegressionResult>& regression_results,
    const std::vector<std::string>& factor_names) {
    
    std::stringstream report;
    report << "=== Factor Attribution Report ===\n\n";
    
    if (regression_results.empty()) {
        report << "No regression results available.\n";
        return report.str();
    }
    
    int num_factors = factor_names.size();
    int num_periods = regression_results.size();
    
    // Compute average factor returns
    std::vector<double> avg_returns(num_factors, 0.0);
    std::vector<double> avg_t_stats(num_factors, 0.0);
    std::vector<double> avg_r_squared(num_factors, 0.0);
    
    for (const auto& result : regression_results) {
        for (int i = 0; i < num_factors && i < result.factor_returns.size(); ++i) {
            avg_returns[i] += result.factor_returns[i];
            avg_t_stats[i] += result.factor_t_stats[i];
            avg_r_squared[i] += result.factor_r_squared[i];
        }
    }
    
    for (int i = 0; i < num_factors; ++i) {
        avg_returns[i] /= num_periods;
        avg_t_stats[i] /= num_periods;
        avg_r_squared[i] /= num_periods;
    }
    
    // Report summary statistics
    report << "Periods analyzed: " << num_periods << "\n";
    report << "Average observations per period: " 
           << std::accumulate(regression_results.begin(), regression_results.end(), 0,
                             [](int sum, const RegressionResult& r) { return sum + r.num_observations; }) / num_periods
           << "\n\n";
    
    report << "Factor Performance Summary:\n";
    report << std::setw(15) << "Factor" 
           << std::setw(15) << "Avg Return" 
           << std::setw(15) << "Avg T-Stat" 
           << std::setw(15) << "Avg R²" << "\n";
    report << std::string(60, '-') << "\n";
    
    for (int i = 0; i < num_factors; ++i) {
        std::string factor_name = (i < factor_names.size()) ? factor_names[i] : "Factor_" + std::to_string(i);
        report << std::setw(15) << factor_name
               << std::setw(15) << std::fixed << std::setprecision(6) << avg_returns[i]
               << std::setw(15) << std::fixed << std::setprecision(3) << avg_t_stats[i]
               << std::setw(15) << std::fixed << std::setprecision(4) << avg_r_squared[i] << "\n";
    }
    
    // Report time series statistics
    report << "\nTime Series Statistics:\n";
    double avg_total_r_squared = 0.0;
    for (const auto& result : regression_results) {
        avg_total_r_squared += result.total_r_squared;
    }
    avg_total_r_squared /= num_periods;
    
    report << "Average Total R²: " << std::fixed << std::setprecision(4) << avg_total_r_squared << "\n";
    
    return report.str();
}

// Private helper methods

std::vector<double> CrossSectionalRegression::compute_ols_estimates(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& y) {
    
    int n = y.size();
    int p = X.size();
    
    if (n == 0 || p == 0) return {};
    
    // Compute X'X matrix
    std::vector<std::vector<double>> XtX(p, std::vector<double>(p, 0.0));
    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) {
            for (int k = 0; k < n; ++k) {
                XtX[i][j] += X[i][k] * X[j][k];
            }
        }
    }
    
    // Compute X'y vector
    std::vector<double> Xty(p, 0.0);
    for (int i = 0; i < p; ++i) {
        for (int k = 0; k < n; ++k) {
            Xty[i] += X[i][k] * y[k];
        }
    }
    
    // Solve (X'X)β = X'y using simple Gaussian elimination
    std::vector<double> beta = Xty;
    std::vector<std::vector<double>> A = XtX;
    
    // Forward elimination
    for (int i = 0; i < p; ++i) {
        // Find pivot
        int max_row = i;
        for (int k = i + 1; k < p; ++k) {
            if (std::abs(A[k][i]) > std::abs(A[max_row][i])) {
                max_row = k;
            }
        }
        
        // Swap rows
        if (max_row != i) {
            std::swap(A[i], A[max_row]);
            std::swap(beta[i], beta[max_row]);
        }
        
        // Eliminate column
        for (int k = i + 1; k < p; ++k) {
            double factor = A[k][i] / A[i][i];
            beta[k] -= factor * beta[i];
            for (int j = i; j < p; ++j) {
                A[k][j] -= factor * A[i][j];
            }
        }
    }
    
    // Back substitution
    for (int i = p - 1; i >= 0; --i) {
        for (int j = i + 1; j < p; ++j) {
            beta[i] -= A[i][j] * beta[j];
        }
        beta[i] /= A[i][i];
    }
    
    return beta;
}

std::vector<double> CrossSectionalRegression::compute_standard_errors(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& residuals,
    int num_observations) {
    
    int p = X.size();
    if (p == 0 || residuals.empty()) return {};
    
    // Compute residual variance
    double residual_variance = 0.0;
    for (double residual : residuals) {
        residual_variance += residual * residual;
    }
    residual_variance /= (num_observations - p);
    
    // Compute (X'X)^(-1) diagonal elements
    std::vector<double> std_errors(p);
    for (int i = 0; i < p; ++i) {
        double sum = 0.0;
        for (int k = 0; k < num_observations; ++k) {
            sum += X[i][k] * X[i][k];
        }
        std_errors[i] = std::sqrt(residual_variance / sum);
    }
    
    return std_errors;
}

std::vector<double> CrossSectionalRegression::compute_t_statistics(
    const std::vector<double>& coefficients,
    const std::vector<double>& std_errors) {
    
    std::vector<double> t_stats;
    t_stats.reserve(coefficients.size());
    
    for (size_t i = 0; i < coefficients.size(); ++i) {
        if (i < std_errors.size() && std_errors[i] != 0.0) {
            t_stats.push_back(coefficients[i] / std_errors[i]);
        } else {
            t_stats.push_back(0.0);
        }
    }
    
    return t_stats;
}

double CrossSectionalRegression::compute_r_squared(
    const std::vector<double>& y,
    const std::vector<double>& y_pred) {
    
    if (y.empty() || y_pred.empty() || y.size() != y_pred.size()) {
        return 0.0;
    }
    
    double y_mean = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
    
    double ss_res = 0.0;  // Sum of squared residuals
    double ss_tot = 0.0;  // Total sum of squares
    
    for (size_t i = 0; i < y.size(); ++i) {
        double residual = y[i] - y_pred[i];
        ss_res += residual * residual;
        
        double deviation = y[i] - y_mean;
        ss_tot += deviation * deviation;
    }
    
    if (ss_tot == 0.0) return 0.0;
    return 1.0 - (ss_res / ss_tot);
}

std::vector<double> CrossSectionalRegression::compute_residuals(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& y,
    const std::vector<double>& coefficients) {
    
    std::vector<double> residuals = y;
    
    for (size_t i = 0; i < y.size(); ++i) {
        for (size_t j = 0; j < coefficients.size() && j < X.size(); ++j) {
            residuals[i] -= X[j][i] * coefficients[j];
        }
    }
    
    return residuals;
}

std::pair<std::vector<std::vector<double>>, std::vector<double>> 
CrossSectionalRegression::prepare_regression_data(
    const std::shared_ptr<arrow::Table>& factor_table,
    const std::string& return_column,
    const std::vector<std::string>& factor_columns,
    int start_row, int end_row) {
    
    std::vector<std::vector<double>> X;
    std::vector<double> y;
    
    if (!factor_table) return {X, y};
    
    int num_rows = std::min(static_cast<int64_t>(end_row), factor_table->num_rows()) - start_row;
    if (num_rows <= 0) return {X, y};
    
    // Initialize X matrix
    X.resize(factor_columns.size());
    for (auto& col : X) {
        col.resize(num_rows);
    }
    
    y.resize(num_rows);
    
    // Extract return column
    auto return_array = factor_table->GetColumnByName(return_column);
    if (!return_array) return {X, y};
    
    // Handle both ChunkedArray and regular Array
    std::shared_ptr<arrow::DoubleArray> return_double_array;
    if (return_array->type()->id() == arrow::Type::DOUBLE) {
        if (return_array->num_chunks() == 1) {
            return_double_array = std::static_pointer_cast<arrow::DoubleArray>(return_array->chunk(0));
        } else {
            // For chunked arrays, we'll need to handle this differently
            // For now, just return empty data
            return {X, y};
        }
    } else {
        return {X, y};
    }
    
    // Extract return values
    for (int i = 0; i < num_rows; ++i) {
        if (return_double_array->IsValid(start_row + i)) {
            y[i] = return_double_array->Value(start_row + i);
        } else {
            y[i] = std::numeric_limits<double>::quiet_NaN();
        }
    }
    
    // Extract factor columns
    for (size_t col_idx = 0; col_idx < factor_columns.size(); ++col_idx) {
        auto factor_array = factor_table->GetColumnByName(factor_columns[col_idx]);
        if (!factor_array) continue;
        
        // Handle both ChunkedArray and regular Array for factors
        std::shared_ptr<arrow::DoubleArray> factor_double_array;
        if (factor_array->type()->id() == arrow::Type::DOUBLE) {
            if (factor_array->num_chunks() == 1) {
                factor_double_array = std::static_pointer_cast<arrow::DoubleArray>(factor_array->chunk(0));
            } else {
                continue; // Skip this factor if it's chunked
            }
        } else {
            continue; // Skip non-double factors
        }
        
        for (int i = 0; i < num_rows; ++i) {
            if (factor_double_array->IsValid(start_row + i)) {
                X[col_idx][i] = factor_double_array->Value(start_row + i);
            } else {
                X[col_idx][i] = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }
    
    return {X, y};
}

void CrossSectionalRegression::handle_missing_data(
    std::vector<std::vector<double>>& X,
    std::vector<double>& y) {
    
    if (X.empty() || y.empty()) return;
    
    int n = y.size();
    std::vector<bool> valid_observations(n, true);
    
    // Mark observations with missing data
    for (int i = 0; i < n; ++i) {
        if (std::isnan(y[i])) {
            valid_observations[i] = false;
            continue;
        }
        
        for (const auto& factor_col : X) {
            if (i < factor_col.size() && std::isnan(factor_col[i])) {
                valid_observations[i] = false;
                break;
            }
        }
    }
    
    // Remove invalid observations
    std::vector<double> y_clean;
    std::vector<std::vector<double>> X_clean(X.size());
    
    for (int i = 0; i < n; ++i) {
        if (valid_observations[i]) {
            y_clean.push_back(y[i]);
            for (size_t j = 0; j < X.size(); ++j) {
                X_clean[j].push_back(X[j][i]);
            }
        }
    }
    
    y = y_clean;
    X = X_clean;
}

void CrossSectionalRegression::winsorize_data(
    std::vector<std::vector<double>>& X,
    std::vector<double>& y,
    double percentile) {
    
    if (y.empty()) return;
    
    // Winsorize returns
    std::vector<double> y_sorted = y;
    std::sort(y_sorted.begin(), y_sorted.end());
    
    int lower_idx = static_cast<int>(percentile * y.size());
    int upper_idx = static_cast<int>((1.0 - percentile) * y.size());
    
    double lower_bound = y_sorted[lower_idx];
    double upper_bound = y_sorted[upper_idx];
    
    for (double& val : y) {
        if (val < lower_bound) val = lower_bound;
        if (val > upper_bound) val = upper_bound;
    }
    
    // Winsorize factors
    for (auto& factor_col : X) {
        if (factor_col.empty()) continue;
        
        std::vector<double> factor_sorted = factor_col;
        std::sort(factor_sorted.begin(), factor_sorted.end());
        
        int lower_idx = static_cast<int>(percentile * factor_col.size());
        int upper_idx = static_cast<int>((1.0 - percentile) * factor_col.size());
        
        double lower_bound = factor_sorted[lower_idx];
        double upper_bound = factor_sorted[upper_idx];
        
        for (double& val : factor_col) {
            if (val < lower_bound) val = lower_bound;
            if (val > upper_bound) val = upper_bound;
        }
    }
}

} // namespace qse 