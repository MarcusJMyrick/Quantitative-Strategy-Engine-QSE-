#pragma once

#include <gmock/gmock.h>
#include "qse/data/IDataReader.h"

class MockDataReader : public qse::IDataReader {
public:
    MOCK_METHOD(const std::vector<qse::Tick>&, read_all_ticks, (), (const, override));
    MOCK_METHOD(const std::vector<qse::Bar>&, read_all_bars, (), (const, override));
}; 