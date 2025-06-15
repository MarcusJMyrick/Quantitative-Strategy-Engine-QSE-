#pragma once

#include "DataReader.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace qse {

class CSVDataReader : public DataReader {
public:
    explicit CSVDataReader(const std::string& file_path);
    
    std::vector<Bar> read_all_bars() override;
    size_t get_bar_count() const override;
    std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time) override;
    Bar get_bar(size_t index) const override;

private:
    std::string file_path_;
    std::vector<Bar> bars_;
    void load_bars();
};

} // namespace qse 