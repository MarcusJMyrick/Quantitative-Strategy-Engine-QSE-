#include "gtest/gtest.h"
#include "qse/data/CSVDataReader.h"
#include "qse/data/Data.h"
#include <fstream>
#include <filesystem>

using namespace qse;

static const char* smallTickCSV =
  "timestamp,price,volume\n"
  "1622505601000,100.5,10\n"
  "1622505600000,100.0,20\n"
  "1622505602000,101.0,5\n";

TEST(CSVDataReaderTicksTest, LoadsAndSortsTicks) {
    // 1) write the CSV into a temp file
    std::string fn = "temp_ticks.csv";
    std::ofstream out(fn);
    out << smallTickCSV;
    out.close();

    // 2) load it
    CSVDataReader reader(fn);

    auto const& ticks = reader.read_all_ticks();
    ASSERT_EQ(ticks.size(), 3u);

    // 3) verify timestamps are sorted and values correct
    // Convert timestamp to milliseconds for comparison
    auto timestamp_to_ms = [](const Timestamp& ts) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            ts.time_since_epoch()).count();
    };

    EXPECT_EQ(timestamp_to_ms(ticks[0].timestamp), 1622505600000LL);
    EXPECT_DOUBLE_EQ(ticks[0].price, 100.0);
    EXPECT_EQ(ticks[0].volume, 20u);

    EXPECT_EQ(timestamp_to_ms(ticks[1].timestamp), 1622505601000LL);
    EXPECT_DOUBLE_EQ(ticks[1].price, 100.5);
    EXPECT_EQ(ticks[1].volume, 10u);

    EXPECT_EQ(timestamp_to_ms(ticks[2].timestamp), 1622505602000LL);
    EXPECT_DOUBLE_EQ(ticks[2].price, 101.0);
    EXPECT_EQ(ticks[2].volume, 5u);

    // clean up
    std::filesystem::remove(fn);
}

TEST(CSVDataReaderTicksTest, HandlesEmptyFile) {
    // Create empty tick file
    std::string fn = "empty_ticks.csv";
    std::ofstream out(fn);
    out << "timestamp,price,volume\n";
    out.close();

    CSVDataReader reader(fn);
    auto const& ticks = reader.read_all_ticks();
    EXPECT_EQ(ticks.size(), 0u);

    // clean up
    std::filesystem::remove(fn);
}

TEST(CSVDataReaderTicksTest, HandlesBarFile) {
    // Create bar file to ensure it doesn't interfere with tick parsing
    std::string fn = "bar_file.csv";
    std::ofstream out(fn);
    out << "timestamp,Open,High,Low,Close,Volume\n";
    out << "1622505600000,100.0,101.0,99.0,100.5,1000\n";
    out.close();

    CSVDataReader reader(fn);
    auto const& ticks = reader.read_all_ticks();
    auto const& bars = reader.read_all_bars();
    
    EXPECT_EQ(ticks.size(), 0u);  // Should not parse ticks from bar file
    EXPECT_EQ(bars.size(), 1u);   // Should parse bars

    // clean up
    std::filesystem::remove(fn);
} 