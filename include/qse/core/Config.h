#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace qse {

/**
 * @brief Configuration class for loading and managing QSE settings
 * 
 * Handles loading of YAML configuration files including:
 * - Symbol-specific slippage coefficients
 * - Backtester settings
 * - Data paths
 */
class Config {
public:
    /**
     * @brief Load configuration from YAML file
     * @param config_path Path to the YAML configuration file
     * @return true if successful, false otherwise
     */
    bool load_config(const std::string& config_path);
    
    /**
     * @brief Get slippage coefficient for a symbol
     * @param symbol The symbol to get slippage for
     * @return Slippage coefficient, or 0.0 if not found
     */
    double get_slippage_coeff(const std::string& symbol) const;
    
    /**
     * @brief Get initial cash amount
     * @return Initial cash amount
     */
    double get_initial_cash() const { return initial_cash_; }
    
    /**
     * @brief Get commission rate
     * @return Commission rate as decimal
     */
    double get_commission_rate() const { return commission_rate_; }
    
    /**
     * @brief Get minimum trade size
     * @return Minimum trade size
     */
    int get_min_trade_size() const { return min_trade_size_; }
    
    /**
     * @brief Get data base path
     * @return Data base path
     */
    std::string get_data_base_path() const { return data_base_path_; }
    
    /**
     * @brief Get processed data path
     * @return Processed data path
     */
    std::string get_processed_data_path() const { return processed_data_path_; }
    
    /**
     * @brief Get results path
     * @return Results path
     */
    std::string get_results_path() const { return results_path_; }

    std::string order_style = "market"; // or "target_percent"
    double min_qty = 0.0;

private:
    // Slippage coefficients per symbol
    std::unordered_map<std::string, double> linear_impact_;
    
    // Backtester settings
    double initial_cash_ = 100000.0;
    double commission_rate_ = 0.001;
    int min_trade_size_ = 1;
    
    // Data paths
    std::string data_base_path_ = "./data";
    std::string processed_data_path_ = "./data/processed";
    std::string results_path_ = "./results";
};

} // namespace qse 