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

    // Get direct pointers to the specific array types for each column
    auto timestamp_col = std::static_pointer_cast<arrow::TimestampArray>(table_->column(0)->chunk(0));
    auto open_col      = std::static_pointer_cast<arrow::DoubleArray>(table_->column(1)->chunk(0));
    auto high_col      = std::static_pointer_cast<arrow::DoubleArray>(table_->column(2)->chunk(0));
    auto low_col       = std::static_pointer_cast<arrow::DoubleArray>(table_->column(3)->chunk(0));
    auto close_col     = std::static_pointer_cast<arrow::DoubleArray>(table_->column(4)->chunk(0));
    auto volume_col    = std::static_pointer_cast<arrow::Int64Array>(table_->column(5)->chunk(0));

    size_t num_rows = table_->num_rows();
    bars.reserve(num_rows);

    // Debug output for first row
    if (num_rows > 0) {
        std::cout << "First row values:" << std::endl;
        std::cout << "Timestamp: " << timestamp_col->Value(0) << std::endl;
        std::cout << "Open: " << open_col->Value(0) << std::endl;
        std::cout << "High: " << high_col->Value(0) << std::endl;
        std::cout << "Low: " << low_col->Value(0) << std::endl;
        std::cout << "Close: " << close_col->Value(0) << std::endl;
        std::cout << "Volume: " << volume_col->Value(0) << std::endl;
    }

    for (int64_t i = 0; i < num_rows; ++i) {
        Bar bar;
        
        // Convert timestamp from nanoseconds to system_clock time_point
        auto timestamp_ns = std::chrono::nanoseconds(timestamp_col->Value(i));
        bar.timestamp = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(timestamp_ns)
        );
        
        // Read numeric values
        bar.open   = open_col->Value(i);
        bar.high   = high_col->Value(i);
        bar.low    = low_col->Value(i);
        bar.close  = close_col->Value(i);
        bar.volume = static_cast<double>(volume_col->Value(i));

        bars.push_back(bar);
    }

    return bars;
}

size_t DataReader::get_bar_count() const {
    return table_ ? static_cast<size_t>(table_->num_rows()) : 0;
}

} // namespace qse