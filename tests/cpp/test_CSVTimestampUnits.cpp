#include "qse/data/CSVDataReader.h"
#include "gtest/gtest.h"
#include <chrono>

using namespace qse;

TEST(CSVDataReaderTicksTest, TimestampUnits) {
    CSVDataReader rdr("tests/cpp/sample_tick_seconds.csv", "TEST");
    const auto& ticks = rdr.read_all_ticks();
    ASSERT_FALSE(ticks.empty());
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ticks.front().timestamp.time_since_epoch()).count();
    // Expect after 1.5e12 ms (2017-07-14) to verify seconds → ms promotion happened
    ASSERT_GT(ts_ms, 1'500'000'000'000LL);
} 