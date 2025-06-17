#pragma once

#include <gmock/gmock.h>
#include "IDataReader.h"

class MockDataReader : public qse::IDataReader {
public:
    MOCK_METHOD(std::vector<qse::Bar>, read_all_bars, (), (override));
    MOCK_METHOD(size_t, get_bar_count, (), (const, override));
    MOCK_METHOD(std::vector<qse::Bar>, read_bars_in_range, 
        (qse::Timestamp start_time, qse::Timestamp end_time), (override));
    MOCK_METHOD(qse::Bar, get_bar, (size_t index), (const, override));
}; 