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
DataReader::DataReader(const std::string& file_path) {
    std::shared_ptr<arrow::io::ReadableFile> infile;
    auto result = arrow::io::ReadableFile::Open(file_path);
    if (!result.ok()) {
        throw std::runtime_error("Failed to open Parquet file: " + result.status().ToString());
    }
    infile = *result;

    std::unique_ptr<parquet::arrow::FileReader> reader;
    auto reader_status = parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    if (!reader_status.ok()) {
        throw std::runtime_error("Failed to create Parquet file reader: " + reader_status.ToString());
    }

    auto table_status = reader->ReadTable(&table_);
    if (!table_status.ok()) {
        throw std::runtime_error("Failed to read Parquet table: " + table_status.ToString());
    }

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
    if (!table_) {
        return bars;
    }

    for (int64_t i = 0; i < table_->num_rows(); ++i) {
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

// This is the implementation the linker was looking for.
Bar DataReader::convert_row_to_bar(int64_t row_index) const {
    auto timestamp_col = std::static_pointer_cast<arrow::TimestampArray>(table_->column(0)->chunk(0));
    auto open_col      = std::static_pointer_cast<arrow::DoubleArray>(table_->column(1)->chunk(0));
    auto high_col      = std::static_pointer_cast<arrow::DoubleArray>(table_->column(2)->chunk(0));
    auto low_col       = std::static_pointer_cast<arrow::DoubleArray>(table_->column(3)->chunk(0));
    auto close_col     = std::static_pointer_cast<arrow::DoubleArray>(table_->column(4)->chunk(0));
    auto volume_col    = std::static_pointer_cast<arrow::Int64Array>(table_->column(5)->chunk(0));

    auto timestamp_ns = std::chrono::nanoseconds(timestamp_col->Value(row_index));

    Bar bar;
    bar.timestamp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(timestamp_ns)
    );
    bar.open   = open_col->Value(row_index);
    bar.high   = high_col->Value(row_index);
    bar.low    = low_col->Value(row_index);
    bar.close  = close_col->Value(row_index);
    bar.volume = static_cast<double>(volume_col->Value(row_index));

    return bar;
}

} // namespace qse