#include "qse/data/CSVDataReader.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdio>
#include <fstream>

using namespace qse;

TEST(CSVDataReaderTicksTest, TimestampUnits) {
    // Self-contained fixture: timestamps written in *seconds* to exercise
    // the reader's seconds -> milliseconds promotion
    const std::string fn = "sample_tick_seconds.csv";
    {
        std::ofstream out(fn);
        out << "timestamp,price,volume\n"
            << "1500000000,100.0,10\n"
            << "1500000001,100.5,20\n";
    }

    CSVDataReader rdr(fn, "TEST");
    const auto& ticks = rdr.read_all_ticks();
    std::remove(fn.c_str());

    ASSERT_FALSE(ticks.empty());
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ticks.front().timestamp.time_since_epoch()).count();
    // Expect >= 1.5e12 ms (2017-07-14) to verify seconds → ms promotion happened
    // 1500000000 seconds * 1000 = 1500000000000 milliseconds
    ASSERT_GE(ts_ms, 1500000000000LL);
}
