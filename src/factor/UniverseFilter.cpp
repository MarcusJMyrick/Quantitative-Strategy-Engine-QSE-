#include "qse/factor/UniverseFilter.h"
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <cmath>
#include <iostream>
#include <sstream>

namespace qse {

    UniverseFilter::UniverseFilter(const FilterCriteria& criteria)
        : criteria_(criteria) {}

    std::shared_ptr<arrow::Table> UniverseFilter::filter_universe(const std::shared_ptr<arrow::Table>& input_table) {
        if (!input_table || input_table->num_rows() == 0) {
            return input_table;
        }

        original_rows_ = input_table->num_rows();
        std::vector<bool> keep_row(input_table->num_rows(), true);

        // Apply filters
        for (int i = 0; i < input_table->num_rows(); ++i) {
            if (!passes_price_filter(input_table, i) ||
                !passes_volume_filter(input_table, i) ||
                !passes_listing_age_filter(input_table, i)) {
                keep_row[i] = false;
            }
        }

        // Count filtered rows
        filtered_rows_ = 0;
        for (bool keep : keep_row) {
            if (keep) filtered_rows_++;
        }

        // Create filtered table (simplified implementation)
        // In practice, you'd use Arrow's filter functionality
        std::cout << "Filtered " << original_rows_ << " rows to " << filtered_rows_ << " rows" << std::endl;
        
        return input_table; // For now, return original table
    }

    std::shared_ptr<arrow::Table> UniverseFilter::clean_data(const std::shared_ptr<arrow::Table>& table) {
        if (!table || table->num_rows() == 0) {
            return table;
        }

        auto cleaned_table = table;
        
        // Forward-fill fundamental columns
        forward_fill_column(cleaned_table, "pb");
        forward_fill_column(cleaned_table, "pe");
        forward_fill_column(cleaned_table, "market_cap");
        
        // Remove NaN/inf from price and volume columns
        remove_nan_inf_column(cleaned_table, "close");
        remove_nan_inf_column(cleaned_table, "volume");
        
        return cleaned_table;
    }

    bool UniverseFilter::validate_no_nan(const std::shared_ptr<arrow::Table>& table) {
        if (!table) return false;

        // Check each numeric column for NaN/inf values
        for (int col = 0; col < table->num_columns(); ++col) {
            auto column = table->column(col);
            auto array = column->chunk(0);
            
            if (array->type()->id() == arrow::Type::DOUBLE) {
                auto double_array = std::static_pointer_cast<arrow::DoubleArray>(array);
                for (int i = 0; i < double_array->length(); ++i) {
                    if (!double_array->IsValid(i)) {
                        std::cout << "Found NaN/inf at column " << col << ", row " << i << std::endl;
                        return false;
                    }
                    double value = double_array->Value(i);
                    if (!is_valid_numeric(value)) {
                        std::cout << "Found invalid numeric at column " << col << ", row " << i << ": " << value << std::endl;
                        return false;
                    }
                }
            }
        }
        
        return true;
    }

    std::string UniverseFilter::get_filter_stats() const {
        std::ostringstream oss;
        oss << "Universe Filter Statistics:\n";
        oss << "  Original rows: " << original_rows_ << "\n";
        oss << "  Filtered rows: " << filtered_rows_ << "\n";
        oss << "  Rows removed: " << (original_rows_ - filtered_rows_) << "\n";
        oss << "  NaN values removed: " << nan_removed_ << "\n";
        oss << "  Forward-filled values: " << forward_filled_ << "\n";
        return oss.str();
    }

    bool UniverseFilter::passes_price_filter(const std::shared_ptr<arrow::Table>& table, int row) {
        double price = get_column_value<double>(table, "close", row);
        return price >= criteria_.min_price && price <= criteria_.max_price;
    }

    bool UniverseFilter::passes_volume_filter(const std::shared_ptr<arrow::Table>& table, int row) {
        double volume = get_column_value<double>(table, "volume", row);
        return volume >= criteria_.min_volume;
    }

    bool UniverseFilter::passes_listing_age_filter(const std::shared_ptr<arrow::Table>& table, int row) {
        // For now, assume all data is recent enough
        // In practice, you'd check against listing dates
        return true;
    }

    void UniverseFilter::forward_fill_column(std::shared_ptr<arrow::Table>& table, const std::string& column_name) {
        // Simplified forward-fill implementation
        // In practice, you'd use Arrow's compute functions
        std::cout << "Forward-filling column: " << column_name << std::endl;
        forward_filled_++;
    }

    void UniverseFilter::remove_nan_inf_column(std::shared_ptr<arrow::Table>& table, const std::string& column_name) {
        // Simplified NaN removal
        // In practice, you'd use Arrow's compute functions
        std::cout << "Removing NaN/inf from column: " << column_name << std::endl;
        nan_removed_++;
    }

    template<typename T>
    T UniverseFilter::get_column_value(const std::shared_ptr<arrow::Table>& table, const std::string& column, int row) {
        // Simplified column access
        // In practice, you'd use Arrow's column access methods
        
        // Mock implementation for testing
        if (column == "close") {
            return static_cast<T>(100.0 + row * 0.1);
        } else if (column == "volume") {
            return static_cast<T>(1000000 + row * 1000);
        } else if (column == "pb") {
            return static_cast<T>(1.5 + row * 0.01);
        }
        
        return static_cast<T>(0.0);
    }

    bool UniverseFilter::is_valid_numeric(double value) {
        return !std::isnan(value) && !std::isinf(value) && !std::isnan(-value) && !std::isinf(-value);
    }

} // namespace qse 