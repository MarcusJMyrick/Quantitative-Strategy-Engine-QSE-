#pragma once

#include "Data.h"
#include <vector>

namespace qse {

class IDataReader {
public:
    virtual ~IDataReader() = default;

    // These are "pure virtual" functions, defining the interface
    virtual std::vector<Bar> read_all_bars() = 0;
    virtual size_t get_bar_count() const = 0;
    virtual std::vector<Bar> read_bars_in_range(Timestamp start_time, Timestamp end_time) = 0;
};

} // namespace qse 