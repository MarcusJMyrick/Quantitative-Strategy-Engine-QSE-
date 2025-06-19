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

class ParquetDataReader : public IDataReader {
public:
    explicit ParquetDataReader(const std::string& file_path);
    std::vector<Bar> read_all_bars() override;
    size_t get_bar_count() const override;
    Bar get_bar(size_t index) const override;
    std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time) override;

private:
    void load_bars();
    std::string file_path_;
    std::vector<Bar> bars_;
};

} // namespace qse 