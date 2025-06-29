#pragma once

#include <unordered_map>
#include <string>
#include <chrono>
#include <optional>

namespace qse {

/**
 * @brief Helper class for loading daily factor weights from CSV files.
 * 
 * Handles the YYYYMMDD filename pattern and provides robust file loading
 * with graceful handling of missing files.
 */
class WeightsLoader {
public:
    /**
     * @brief Load weights for a specific date
     * @param base_path Base directory path for weight files
     * @param date Date to load weights for
     * @return Optional map of symbol to weight, empty if file not found
     */
    static std::optional<std::unordered_map<std::string, double>> 
    load_daily_weights(const std::string& base_path, 
                      const std::chrono::system_clock::time_point& date);
    
    /**
     * @brief Generate filename for a specific date
     * @param base_path Base directory path
     * @param date Date to generate filename for
     * @return Full path to weights file (e.g., "path/weights_20241215.csv")
     */
    static std::string generate_filename(const std::string& base_path,
                                       const std::chrono::system_clock::time_point& date);
    
    /**
     * @brief Load weights from a specific file path
     * @param file_path Full path to weights CSV file
     * @return Optional map of symbol to weight, empty if file not found/invalid
     */
    static std::optional<std::unordered_map<std::string, double>> 
    load_weights_from_file(const std::string& file_path);

    /**
     * @brief Convert timestamp to YYYYMMDD string
     * @param date Date to convert
     * @return YYYYMMDD formatted string
     */
    static std::string date_to_string(const std::chrono::system_clock::time_point& date);

private:
};

} // namespace qse 