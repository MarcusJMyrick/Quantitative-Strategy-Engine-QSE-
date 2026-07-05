#pragma once
#include "qse/data/IDataReader.h"
#include <cstddef>
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

    // --- Data-quality report (populated during load) ---

    // Rows that could not be parsed (missing/non-numeric fields) and were
    // skipped instead of aborting the whole load
    std::size_t skipped_row_count() const { return skipped_rows_; }

    // Missing rows in the time grid, inferred from the median spacing of the
    // loaded series (exact for grid data; heuristic for event-time ticks)
    std::size_t gap_count() const { return gap_count_; }

private:
    void load_data(); // Renamed to be more generic
    std::string file_path_;
    std::string symbol_override_;

    // The reader now stores both ticks and bars.
    std::vector<Bar> bars_;
    std::vector<Tick> ticks_;

    std::size_t skipped_rows_ = 0;
    std::size_t gap_count_ = 0;
};

} // namespace qse