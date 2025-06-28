#pragma once
#include <string>
#include <memory>
#include <vector>
#include "qse/factor/UniverseFilter.h"

namespace arrow {
    class Table;
}

namespace qse {

/**
 * @class MultiFactorCalculator
 * @brief Computes multi-factor model factors from daily OHLCV data
 * 
 * This class implements the factor computation pipeline:
 * 1. Load daily price data
 * 2. Apply universe filters and data hygiene
 * 3. Compute momentum, volatility, and value factors
 * 4. Apply winsorization and z-score normalization
 * 5. Create composite alpha scores
 * 6. Output to Parquet format
 */
class MultiFactorCalculator {
public:
    MultiFactorCalculator() = default;
    ~MultiFactorCalculator() = default;

    /**
     * @brief Compute factors from input CSV and save to Parquet
     * @param in_csv Input CSV file path with daily OHLCV data
     * @param out_parquet Output Parquet file path for factors
     * @param weights_yaml YAML config file with factor weights
     */
    void compute_factors(const std::string& in_csv,
                        const std::string& out_parquet,
                        const std::string& weights_yaml);

    /**
     * @brief Set universe filter criteria
     * @param min_price Minimum stock price
     * @param min_volume Minimum daily volume
     * @param min_listing_age Minimum days since listing
     * @param max_price Maximum stock price
     */
    void set_filter_criteria(double min_price, double min_volume, int min_listing_age, double max_price);

private:
    // Helper methods for Arrow table operations
    std::shared_ptr<arrow::Table> load_arrow_table(const std::string& csv_path);
    void save_parquet(const std::shared_ptr<arrow::Table>& table, const std::string& path);
    void append_column(const std::shared_ptr<arrow::Table>& table, 
                      const std::string& name, const std::vector<double>& data);
    
    template<typename T>
    T col(const std::shared_ptr<arrow::Table>& table, const std::string& name, int row);

    // Universe filter
    std::unique_ptr<UniverseFilter> universe_filter_;
};

} // namespace qse 