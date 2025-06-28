#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace arrow {
    class Table;
}

namespace qse {

/**
 * @class CrossSectionalRegression
 * @brief Implements Barra-style cross-sectional regression for factor analysis
 * 
 * This class performs cross-sectional regression to:
 * 1. Compute factor exposures and returns
 * 2. Calculate factor risk and alpha decomposition
 * 3. Generate factor attribution reports
 * 4. Handle missing data and outliers
 */
class CrossSectionalRegression {
public:
    CrossSectionalRegression() = default;
    ~CrossSectionalRegression() = default;

    /**
     * @brief Run cross-sectional regression on factor data
     * @param factor_table Arrow table with factor exposures and returns
     * @param date_column Name of the date column
     * @param return_column Name of the return column
     * @param factor_columns Vector of factor column names
     * @return Regression results including factor returns and statistics
     */
    struct RegressionResult {
        std::vector<double> factor_returns;           // Factor returns for the period
        std::vector<double> factor_std_errors;        // Standard errors
        std::vector<double> factor_t_stats;           // T-statistics
        std::vector<double> factor_r_squared;         // R-squared values
        std::vector<double> residuals;                // Regression residuals
        double total_r_squared;                       // Overall R-squared
        int num_observations;                         // Number of observations
        int num_factors;                              // Number of factors
    };

    RegressionResult run_regression(const std::shared_ptr<arrow::Table>& factor_table,
                                   const std::string& date_column,
                                   const std::string& return_column,
                                   const std::vector<std::string>& factor_columns);

    /**
     * @brief Compute rolling cross-sectional regression over time
     * @param factor_table Arrow table with time series factor data
     * @param date_column Name of the date column
     * @param return_column Name of the return column
     * @param factor_columns Vector of factor column names
     * @param window_size Rolling window size (in periods)
     * @return Time series of regression results
     */
    std::vector<RegressionResult> run_rolling_regression(
        const std::shared_ptr<arrow::Table>& factor_table,
        const std::string& date_column,
        const std::string& return_column,
        const std::vector<std::string>& factor_columns,
        int window_size = 252);

    /**
     * @brief Compute factor risk decomposition
     * @param factor_returns Time series of factor returns
     * @param factor_exposures Factor exposure matrix
     * @return Risk decomposition statistics
     */
    struct RiskDecomposition {
        std::vector<double> factor_variances;         // Factor variances
        std::vector<double> factor_covariances;       // Factor covariances
        std::vector<double> factor_contributions;     // Risk contributions
        double total_portfolio_variance;              // Total portfolio variance
        double specific_variance;                     // Specific (idiosyncratic) variance
    };

    RiskDecomposition compute_risk_decomposition(
        const std::vector<std::vector<double>>& factor_returns,
        const std::vector<std::vector<double>>& factor_exposures);

    /**
     * @brief Generate factor attribution report
     * @param regression_results Time series regression results
     * @param factor_names Names of the factors
     * @return Attribution report as string
     */
    std::string generate_attribution_report(
        const std::vector<RegressionResult>& regression_results,
        const std::vector<std::string>& factor_names);

private:
    // Helper methods for regression computation
    std::vector<double> compute_ols_estimates(const std::vector<std::vector<double>>& X,
                                             const std::vector<double>& y);
    
    std::vector<double> compute_standard_errors(const std::vector<std::vector<double>>& X,
                                               const std::vector<double>& residuals,
                                               int num_observations);
    
    std::vector<double> compute_t_statistics(const std::vector<double>& coefficients,
                                            const std::vector<double>& std_errors);
    
    double compute_r_squared(const std::vector<double>& y,
                            const std::vector<double>& y_pred);
    
    std::vector<double> compute_residuals(const std::vector<std::vector<double>>& X,
                                         const std::vector<double>& y,
                                         const std::vector<double>& coefficients);

    // Data preprocessing
    std::pair<std::vector<std::vector<double>>, std::vector<double>> 
    prepare_regression_data(const std::shared_ptr<arrow::Table>& factor_table,
                           const std::string& return_column,
                           const std::vector<std::string>& factor_columns,
                           int start_row, int end_row);

    // Handle missing data
    void handle_missing_data(std::vector<std::vector<double>>& X,
                            std::vector<double>& y);
    
    // Remove outliers using winsorization
    void winsorize_data(std::vector<std::vector<double>>& X,
                       std::vector<double>& y,
                       double percentile = 0.01);
};

} // namespace qse 