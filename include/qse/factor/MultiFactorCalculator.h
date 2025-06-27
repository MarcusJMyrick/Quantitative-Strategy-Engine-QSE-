#pragma once
#include <string>
#include <memory>
#include <vector>

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
 * 2. Compute momentum, volatility, and value factors
 * 3. Apply winsorization and z-score normalization
 * 4. Create composite alpha scores
 * 5. Output to Parquet format
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

private:
    // Helper methods for Arrow table operations
    std::shared_ptr<arrow::Table> load_arrow_table(const std::string& csv_path);
    void save_parquet(const std::shared_ptr<arrow::Table>& table, const std::string& path);
    void append_column(const std::shared_ptr<arrow::Table>& table, 
                      const std::string& name, const std::vector<double>& data);
    
    template<typename T>
    T col(const std::shared_ptr<arrow::Table>& table, const std::string& name, int row);
};

} // namespace qse 