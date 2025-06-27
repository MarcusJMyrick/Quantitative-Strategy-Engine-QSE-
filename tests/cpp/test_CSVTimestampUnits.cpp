#include "qse/data/CSVDataReader.h"
#include "gtest/gtest.h"
#include <chrono>

using namespace qse;

TEST(CSVDataReaderTicksTest, TimestampUnits) {
    CSVDataReader rdr("sample_tick_seconds.csv", "TEST");
    const auto& ticks = rdr.read_all_ticks();
    ASSERT_FALSE(ticks.empty());
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ticks.front().timestamp.time_since_epoch()).count();
    // Expect >= 1.5e12 ms (2017-07-14) to verify seconds → ms promotion happened
    // 1500000000 seconds * 1000 = 1500000000000 milliseconds
    ASSERT_GE(ts_ms, 1500000000000LL);
} 