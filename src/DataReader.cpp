#include "DataReader.h"
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>
#include <stdexcept>
#include <arrow/table.h>
#include <arrow/array.h>

namespace qse {

DataReader::DataReader(const std::string& file_path)
    : file_path_(file_path) {
    if (!initialize_reader()) {
        throw std::runtime_error("Failed to initialize Parquet reader");
    }
}

bool DataReader::initialize_reader() {
    try {
        // Open the Parquet file
        std::shared_ptr<arrow::io::ReadableFile> infile;
        PARQUET_THROW_NOT_OK(
            arrow::io::ReadableFile::Open(file_path_).Value(&infile));

        // Create Parquet Arrow FileReader
        std::unique_ptr<parquet::arrow::FileReader> file_reader;
        PARQUET_THROW_NOT_OK(
            parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &file_reader));

        // Read the table
        std::shared_ptr<arrow::Table> table;
        PARQUET_THROW_NOT_OK(file_reader->ReadTable(&table));
        table_ = table;

        return true;
    } catch (const std::exception& e) {
        // Log error and return false
        return false;
    }
}

std::vector<Bar> DataReader::read_all_bars() {
    std::vector<Bar> bars;
    if (!table_) {
        return bars;
    }

    // Get the number of rows
    int64_t num_rows = table_->num_rows();
    bars.reserve(num_rows);

    // Convert each row to a Bar
    for (int64_t i = 0; i < num_rows; ++i) {
        bars.push_back(convert_row_to_bar(i));
    }

    return bars;
}

std::vector<Bar> DataReader::read_bars_in_range(Timestamp start_time, Timestamp end_time) {
    std::vector<Bar> bars;
    if (!table_) {
        return bars;
    }

    // Get the number of rows
    int64_t num_rows = table_->num_rows();
    bars.reserve(num_rows);

    // Convert rows within the time range to Bars
    for (int64_t i = 0; i < num_rows; ++i) {
        Bar bar = convert_row_to_bar(i);
        if (bar.timestamp >= start_time && bar.timestamp <= end_time) {
            bars.push_back(bar);
        }
    }

    return bars;
}

size_t DataReader::get_bar_count() const {
    return table_ ? static_cast<size_t>(table_->num_rows()) : 0;
}

Bar DataReader::convert_row_to_bar(int64_t row_index) const {
    if (!table_) {
        return Bar();
    }

    // Get the columns
    auto timestamp_col = std::static_pointer_cast<arrow::TimestampArray>(table_->column(0)->chunk(0));
    auto open_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(1)->chunk(0));
    auto high_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(2)->chunk(0));
    auto low_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(3)->chunk(0));
    auto close_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(4)->chunk(0));
    auto volume_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(5)->chunk(0));

    // Convert timestamp to system_clock::time_point
    auto timestamp = std::chrono::system_clock::from_time_t(
        timestamp_col->Value(row_index) / 1000000000  // Convert nanoseconds to seconds
    );

    // Create and return the Bar
    return Bar(
        timestamp,
        open_col->Value(row_index),
        high_col->Value(row_index),
        low_col->Value(row_index),
        close_col->Value(row_index),
        static_cast<Volume>(volume_col->Value(row_index))
    );
}

} // namespace qse 