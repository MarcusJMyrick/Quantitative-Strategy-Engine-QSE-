// src/DataReader.cpp

#include "DataReader.h"
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <stdexcept>
#include <arrow/table.h>
#include <arrow/array.h>
#include <iostream> // For debugging

namespace qse {

// The constructor correctly loads the entire table into the `table_` member.
DataReader::DataReader(const std::string& filename) {
    // Open the file
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(filename));
    
    // Create a Parquet reader
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &file_reader_));
    
    // Read the entire file into a table
    PARQUET_THROW_NOT_OK(file_reader_->ReadTable(&table_));

    // Debug output
    std::cout << "Successfully read table with " << table_->num_rows() << " rows and " 
              << table_->num_columns() << " columns" << std::endl;

    // Verify we have the expected columns
    if (table_->num_columns() != 6) {
        throw std::runtime_error("Unexpected number of columns in Parquet file");
    }
}

std::vector<Bar> DataReader::read_all_bars() {
    std::vector<Bar> bars;
    if (!table_) {
        return bars;
    }

    size_t num_rows = table_->num_rows();
    bars.reserve(num_rows);

    for (int64_t i = 0; i < num_rows; ++i) {
        bars.push_back(convert_row_to_bar(i));
    }

    return bars;
}

std::vector<Bar> DataReader::read_bars_in_range(Timestamp start_time, Timestamp end_time) {
    std::vector<Bar> bars;
    size_t count = get_bar_count();
    
    for (size_t i = 0; i < count; ++i) {
        Bar bar = convert_row_to_bar(i);
        if (bar.timestamp >= start_time && bar.timestamp <= end_time) {
            bars.push_back(bar);
        }
    }
    
    return bars;
}

size_t DataReader::get_bar_count() const {
    return table_ ? table_->num_rows() : 0;
}

Bar DataReader::get_bar(size_t index) const {
    if (!table_ || index >= get_bar_count()) {
        throw std::out_of_range("Bar index is out of range.");
    }
    return convert_row_to_bar(index);
}

Bar DataReader::convert_row_to_bar(size_t row_index) const {
    if (!table_ || row_index >= table_->num_rows()) {
        throw std::out_of_range("Row index is out of range.");
    }

    // Get the timestamp column
    auto timestamp_col = std::static_pointer_cast<arrow::TimestampArray>(table_->column(0)->chunk(0));
    auto timestamp_ns = std::chrono::nanoseconds(timestamp_col->Value(row_index));
    auto timestamp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(timestamp_ns)
    );

    // Get the OHLC columns
    auto open_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(1)->chunk(0));
    auto high_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(2)->chunk(0));
    auto low_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(3)->chunk(0));
    auto close_col = std::static_pointer_cast<arrow::DoubleArray>(table_->column(4)->chunk(0));
    auto volume_col = std::static_pointer_cast<arrow::Int64Array>(table_->column(5)->chunk(0));

    return Bar(
        timestamp,
        open_col->Value(row_index),
        high_col->Value(row_index),
        low_col->Value(row_index),
        close_col->Value(row_index),
        static_cast<double>(volume_col->Value(row_index))
    );
}

} // namespace qse