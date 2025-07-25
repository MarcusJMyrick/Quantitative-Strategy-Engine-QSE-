#pragma once
#include "qse/data/IDataReader.h"
#include <vector>
#include <string>
#include "qse/data/Data.h"

namespace qse {

class CSVDataReader : public qse::IDataReader {
public:
    explicit CSVDataReader(const std::string& file_path);
    CSVDataReader(const std::string& file_path, const std::string& symbol_override);
    
    // Implement the new tick reading method.
    const std::vector<Tick>& read_all_ticks() const override;
    
    // We still need to implement the bar reading method from the interface.
    const std::vector<Bar>& read_all_bars() const override;

private:
    void load_data(); // Renamed to be more generic
    std::string file_path_;
    std::string symbol_override_;
    
    // The reader now stores both ticks and bars.
    std::vector<Bar> bars_;
    std::vector<Tick> ticks_; 
};

} // namespace qse