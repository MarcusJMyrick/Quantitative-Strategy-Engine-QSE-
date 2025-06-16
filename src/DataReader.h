// src/DataReader.h

#pragma once

#include "IDataReader.h"
#include <memory>
#include <string>
#include <vector>
#include <arrow/table.h>
#include <parquet/arrow/reader.h>

// Forward-declare the arrow::Table class to keep headers clean
namespace arrow {
class Table;
}

namespace qse {

class DataReader : public IDataReader {
public:
    /**
     * @brief Constructor that opens and reads the Parquet file immediately.
     * @param file_path Path to the Parquet file.
     * @throws std::runtime_error if the file cannot be read.
     */
    explicit DataReader(const std::string& file_path);

    /**
     * @brief Read all bars from the loaded data.
     * @return Vector of all Bar objects.
     */
    std::vector<Bar> read_all_bars() override;

    /**
     * @brief Get the total number of bars in the file.
     * @return Number of bars.
     */
    size_t get_bar_count() const override;

    /**
     * @brief Read bars in a specified range.
     * @param start_time Start time of the range.
     * @param end_time End time of the range.
     * @return Vector of Bar objects in the specified range.
     */
    std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time) override;

    /**
     * @brief Get a specific bar from the loaded data.
     * @param index Index of the bar to retrieve.
     * @return The retrieved Bar object.
     */
    Bar get_bar(size_t index) const override;

private:
    std::unique_ptr<parquet::arrow::FileReader> file_reader_;
    std::shared_ptr<arrow::Table> table_;

    // This is the private helper function we will implement
    Bar convert_row_to_bar(size_t row_index) const;
};

} // namespace qse