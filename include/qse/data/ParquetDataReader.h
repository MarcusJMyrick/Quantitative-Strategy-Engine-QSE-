#pragma once

#include "qse/data/IDataReader.h"
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <memory>
#include <string>
#include <vector>

namespace qse {

class ParquetDataReader : public qse::IDataReader {
public:
    explicit ParquetDataReader(const std::string& file_path);
    
    // Implement the IDataReader interface
    const std::vector<qse::Tick>& read_all_ticks() const override;
    const std::vector<qse::Bar>& read_all_bars() const override;

private:
    void load_data(); // Renamed from load_bars() to be more generic
    std::string file_path_;
    std::vector<qse::Tick> ticks_; // Can be empty if Parquet only contains bars
    std::vector<qse::Bar> bars_;
};

} // namespace qse 