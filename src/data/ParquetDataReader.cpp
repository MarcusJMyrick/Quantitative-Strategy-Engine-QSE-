#include "qse/data/ParquetDataReader.h"
#include <filesystem>
#include <stdexcept>
#include <chrono>

namespace qse {

ParquetDataReader::ParquetDataReader(const std::string& file_path)
    : file_path_(file_path) {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File not found: " + file_path);
    }
    load_bars();
}

void ParquetDataReader::load_bars() {
    try {
        // Open the Parquet file
        std::shared_ptr<arrow::io::ReadableFile> infile;
        PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path_));

        // Create a Parquet reader
        std::unique_ptr<parquet::arrow::FileReader> reader;
        auto result = parquet::arrow::OpenFile(infile, arrow::default_memory_pool());
        PARQUET_THROW_NOT_OK(result.status());
        reader = std::move(result).ValueOrDie();

        // Read the entire file into a Table
        std::shared_ptr<arrow::Table> table;
        PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

        // Get the columns we need
        auto timestamp_col = std::static_pointer_cast<arrow::Int64Array>(table->GetColumnByName("timestamp")->chunk(0));
        auto open_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("open")->chunk(0));
        auto high_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("high")->chunk(0));
        auto low_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("low")->chunk(0));
        auto close_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("close")->chunk(0));
        auto volume_col = std::static_pointer_cast<arrow::Int64Array>(table->GetColumnByName("volume")->chunk(0));
        auto symbol_col = std::static_pointer_cast<arrow::StringArray>(table->GetColumnByName("symbol")->chunk(0));

        // Convert to our Bar format
        for (int64_t i = 0; i < table->num_rows(); ++i) {
            Bar bar;
            bar.symbol = symbol_col->GetString(i);
            bar.timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{timestamp_col->Value(i)}};
            bar.open = open_col->Value(i);
            bar.high = high_col->Value(i);
            bar.low = low_col->Value(i);
            bar.close = close_col->Value(i);
            bar.volume = volume_col->Value(i);
            bars_.push_back(bar);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error reading Parquet file: " + std::string(e.what()));
    }
}

std::vector<Bar> ParquetDataReader::read_all_bars() {
    return bars_;
}

size_t ParquetDataReader::get_bar_count() const {
    return bars_.size();
}

Bar ParquetDataReader::get_bar(size_t index) const {
    if (index >= bars_.size()) {
        throw std::out_of_range("Index out of range");
    }
    return bars_[index];
}

std::vector<Bar> ParquetDataReader::read_bars_in_range(Timestamp start_time, Timestamp end_time) {
    std::vector<Bar> result;
    for (const auto& bar : bars_) {
        if (bar.timestamp >= start_time && bar.timestamp <= end_time) {
            result.push_back(bar);
        }
    }
    return result;
}

} // namespace qse 