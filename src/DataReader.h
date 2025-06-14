#pragma once

#include "Data.h"
#include <memory>
#include <string>
#include <vector>
#include <arrow/api.h>
#include <parquet/arrow/reader.h>

namespace qse {

/**
 * @brief Class responsible for reading market data from Parquet files
 */
class DataReader {
public:
    /**
     * @brief Constructor
     * @param file_path Path to the Parquet file
     */
    explicit DataReader(const std::string& file_path);

    /**
     * @brief Destructor
     */
    ~DataReader() = default;

    /**
     * @brief Read all bars from the Parquet file
     * @return Vector of Bar objects
     */
    std::vector<Bar> read_all_bars();

    /**
     * @brief Read bars within a time range
     * @param start_time Start timestamp
     * @param end_time End timestamp
     * @return Vector of Bar objects within the time range
     */
    std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time);

    /**
     * @brief Get the total number of bars in the file
     * @return Number of bars
     */
    size_t get_bar_count() const;

private:
    std::string file_path_;
    std::shared_ptr<arrow::Table> table_;
    std::shared_ptr<parquet::ParquetFileReader> reader_;

    /**
     * @brief Initialize the Parquet reader
     * @return true if successful, false otherwise
     */
    bool initialize_reader();

    /**
     * @brief Convert a Parquet row to a Bar object
     * @param row_index Index of the row to convert
     * @return Bar object
     */
    Bar convert_row_to_bar(int64_t row_index) const;
};

} // namespace qse 