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
    load_data();
}

void ParquetDataReader::load_data() {
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

        // Check for bar columns
        if (table->GetColumnByName("close")) {
            auto timestamp_col = std::static_pointer_cast<arrow::Int64Array>(table->GetColumnByName("timestamp")->chunk(0));
            auto open_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("open")->chunk(0));
            auto high_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("high")->chunk(0));
            auto low_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("low")->chunk(0));
            auto close_col = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName("close")->chunk(0));
            auto volume_col = std::static_pointer_cast<arrow::Int64Array>(table->GetColumnByName("volume")->chunk(0));
            
            // Check if symbol column exists, otherwise use filename as symbol
            std::string default_symbol = "UNKNOWN";
            if (table->GetColumnByName("symbol")) {
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
            } else {
                // Convert to our Bar format without symbol column
                for (int64_t i = 0; i < table->num_rows(); ++i) {
                    Bar bar;
                    bar.symbol = default_symbol;
                    bar.timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{timestamp_col->Value(i)}};
                    bar.open = open_col->Value(i);
                    bar.high = high_col->Value(i);
                    bar.low = low_col->Value(i);
                    bar.close = close_col->Value(i);
                    bar.volume = volume_col->Value(i);
                    bars_.push_back(bar);
                }
            }
        }
        // You could add a similar check for tick columns ('price', 'volume') here in the future
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error reading Parquet file: " + std::string(e.what()));
    }
}

const std::vector<qse::Tick>& ParquetDataReader::read_all_ticks() const {
    // For now, return empty ticks since ParquetDataReader is bar-focused
    return ticks_;
}

const std::vector<qse::Bar>& ParquetDataReader::read_all_bars() const {
    return bars_;
}

} // namespace qse 