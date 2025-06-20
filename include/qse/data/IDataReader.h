#pragma once
#include "qse/data/Data.h"
#include <vector>
#include <string>

namespace qse {

// This interface defines the contract for any data-reading component.
// It abstracts away the source of the data (e.g., CSV, Parquet, live feed).
class IDataReader {
public:
    virtual ~IDataReader() = default;

    // --- NEW: A method to get all tick data ---
    // This is the core addition for making the engine tick-driven.
    virtual const std::vector<qse::Tick>& read_all_ticks() const = 0;
    
    // --- UPDATED: Returns by const reference for efficiency ---
    // This method remains to support bar-based strategies and tests.
    virtual const std::vector<qse::Bar>& read_all_bars() const = 0;
};

} // namespace qse