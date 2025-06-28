#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

namespace arrow {
    class Table;
}

namespace qse {

/**
 * @struct FilterCriteria
 * @brief Defines filtering criteria for universe selection
 */
struct FilterCriteria {
    double min_price = 5.0;           // Minimum stock price
    double min_volume = 1000000;      // Minimum daily volume
    int min_listing_age_days = 252;   // Minimum days since listing
    double max_price = 10000.0;       // Maximum stock price (penny stocks filter)
    
    FilterCriteria() = default;
    FilterCriteria(double min_p, double min_v, int min_age, double max_p)
        : min_price(min_p), min_volume(min_v), min_listing_age_days(min_age), max_price(max_p) {}
};

/**
 * @class UniverseFilter
 * @brief Handles data hygiene and filtering for multi-factor model universe
 * 
 * This class implements:
 * 1. Price and volume filters
 * 2. Listing age filters
 * 3. NaN/inf removal and forward-fill
 * 4. Data validation and quality checks
 */
class UniverseFilter {
public:
    UniverseFilter() = default;
    explicit UniverseFilter(const FilterCriteria& criteria);
    ~UniverseFilter() = default;

    /**
     * @brief Apply universe filters to input data
     * @param input_table Input Arrow table with price/volume data
     * @return Filtered table with valid securities only
     */
    std::shared_ptr<arrow::Table> filter_universe(const std::shared_ptr<arrow::Table>& input_table);

    /**
     * @brief Clean data by removing NaN/inf values and forward-filling
     * @param table Input table to clean
     * @return Cleaned table with forward-filled values
     */
    std::shared_ptr<arrow::Table> clean_data(const std::shared_ptr<arrow::Table>& table);

    /**
     * @brief Validate that output table contains no NaN values
     * @param table Table to validate
     * @return True if table is valid (no NaN), false otherwise
     */
    bool validate_no_nan(const std::shared_ptr<arrow::Table>& table);

    /**
     * @brief Get statistics about the filtering process
     * @return String with filtering statistics
     */
    std::string get_filter_stats() const;

private:
    // Filtering methods
    bool passes_price_filter(const std::shared_ptr<arrow::Table>& table, int row);
    bool passes_volume_filter(const std::shared_ptr<arrow::Table>& table, int row);
    bool passes_listing_age_filter(const std::shared_ptr<arrow::Table>& table, int row);
    
    // Data cleaning methods
    void forward_fill_column(std::shared_ptr<arrow::Table>& table, const std::string& column_name);
    void remove_nan_inf_column(std::shared_ptr<arrow::Table>& table, const std::string& column_name);
    
    // Utility methods
    template<typename T>
    T get_column_value(const std::shared_ptr<arrow::Table>& table, const std::string& column, int row);
    bool is_valid_numeric(double value);
    
    FilterCriteria criteria_;
    
    // Statistics tracking
    int original_rows_ = 0;
    int filtered_rows_ = 0;
    int nan_removed_ = 0;
    int forward_filled_ = 0;
};

} // namespace qse 