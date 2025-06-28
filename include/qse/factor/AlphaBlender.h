#pragma once

#include <arrow/table.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace qse {

/**
 * @brief AlphaBlender for combining factor scores into final alpha scores
 * 
 * Supports two blending methods:
 * 1. YAML-configured weights per factor
 * 2. IR-weighted blending using Information Ratio
 */
class AlphaBlender {
public:
    struct BlendingConfig {
        std::map<std::string, double> factor_weights;  // YAML weights
        bool use_ir_weighting = false;                 // Use IR-weighted blending
        double ir_lookback_period = 252;               // Days for IR calculation
        double min_ir_weight = 0.1;                    // Minimum weight for any factor
        double max_ir_weight = 2.0;                    // Maximum weight for any factor
    };

    struct BlendingResult {
        std::shared_ptr<arrow::Table> table;           // Table with alpha_score column
        std::map<std::string, double> final_weights;   // Final weights used
        std::map<std::string, double> factor_irs;      // IR values for each factor
    };

    AlphaBlender() = default;
    ~AlphaBlender() = default;

    /**
     * @brief Load blending configuration from YAML file
     * @param config_path Path to YAML configuration file
     * @return true if successful, false otherwise
     */
    bool load_config(const std::string& config_path);

    /**
     * @brief Set blending configuration directly
     * @param config Blending configuration
     */
    void set_config(const BlendingConfig& config);

    /**
     * @brief Blend factor scores into final alpha score
     * @param table Input table with factor columns
     * @param factor_cols Vector of factor column names
     * @param return_col Name of return column for IR calculation
     * @param date_col Name of date column
     * @return BlendingResult with alpha_score column and metadata
     */
    BlendingResult blend_factors(const std::shared_ptr<arrow::Table>& table,
                                const std::vector<std::string>& factor_cols,
                                const std::string& return_col,
                                const std::string& date_col);

    /**
     * @brief Calculate Information Ratio for a factor
     * @param factor_values Factor values
     * @param return_values Return values
     * @return Information Ratio
     */
    double calculate_ir(const std::vector<double>& factor_values,
                       const std::vector<double>& return_values);

    /**
     * @brief Get current configuration
     * @return Current blending configuration
     */
    const BlendingConfig& get_config() const { return config_; }

private:
    BlendingConfig config_;

    /**
     * @brief Calculate IR-weighted factor weights
     * @param table Input table
     * @param factor_cols Factor column names
     * @param return_col Return column name
     * @param date_col Date column name
     * @return Map of factor name to IR weight
     */
    std::map<std::string, double> calculate_ir_weights(
        const std::shared_ptr<arrow::Table>& table,
        const std::vector<std::string>& factor_cols,
        const std::string& return_col,
        const std::string& date_col);

    /**
     * @brief Normalize weights to sum to 1.0
     * @param weights Input weights
     * @return Normalized weights
     */
    std::map<std::string, double> normalize_weights(
        const std::map<std::string, double>& weights);

    /**
     * @brief Apply weights to factor scores
     * @param table Input table
     * @param factor_cols Factor column names
     * @param weights Factor weights
     * @return Table with alpha_score column
     */
    std::shared_ptr<arrow::Table> apply_weights(
        const std::shared_ptr<arrow::Table>& table,
        const std::vector<std::string>& factor_cols,
        const std::map<std::string, double>& weights);
};

} // namespace qse 