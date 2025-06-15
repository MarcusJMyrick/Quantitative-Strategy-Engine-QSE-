// src/DataReader.h

#pragma once

#include "IDataReader.h"
#include <memory>
#include <string>
#include <vector>
#include <arrow/table.h>

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

private:
    std::shared_ptr<arrow::Table> table_;

    /**
     * @brief Helper to convert a single row from the Arrow Table to a Bar.
     * @param row_index The index of the row to convert.
     * @return A populated Bar object.
     */
    Bar convert_row_to_bar(int64_t row_index) const;
};

} // namespace qse