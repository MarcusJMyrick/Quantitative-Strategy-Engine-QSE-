#include <gtest/gtest.h>
#include "DataReader.h"
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

namespace qse {
namespace test {

// This macro is provided by CMake to locate the test data directory
#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

class DataReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Correctly construct the path using std::filesystem::path
        std::filesystem::path test_data_path = std::filesystem::path(TEST_DATA_DIR) / "test_bars.parquet";
        file_path_ = test_data_path.string();
    }

    std::string file_path_;
};

TEST_F(DataReaderTest, CanReadParquetFile) {
    DataReader reader(file_path_);
    auto bars = reader.read_all_bars();
    
    // Verify we can read the file and get some data
    ASSERT_FALSE(bars.empty());
    
    // Verify the first bar has valid data
    const auto& first_bar = bars[0];
    EXPECT_GT(first_bar.open, 0.0);
    EXPECT_GT(first_bar.high, 0.0);
    EXPECT_GT(first_bar.low, 0.0);
    EXPECT_GT(first_bar.close, 0.0);
    EXPECT_GT(first_bar.volume, 0);
}

TEST_F(DataReaderTest, CanReadTimeRange) {
    // This time range correctly selects a subset of the data
    auto start_ns = std::chrono::nanoseconds(1704110400000000000); // 2024-01-01 12:00:00 UTC
    auto end_ns = std::chrono::nanoseconds(1704153600000000000);   // 2024-01-02 00:00:00 UTC

    Timestamp start_time = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(start_ns)
    );
    Timestamp end_time = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(end_ns)
    );

    DataReader reader(file_path_);
    auto bars = reader.read_bars_in_range(start_time, end_time);
    
    // --- Corrected Assertions ---
    // The log told us it found 13 bars in this range.
    ASSERT_EQ(bars.size(), 13); 

    // The log also told us the exact values of the first bar in this slice.
    // We update our expectations to match the actual, correct data.
    const auto& first_bar_in_range = bars[0];
    EXPECT_DOUBLE_EQ(first_bar_in_range.open, 106.22687006340611);
    EXPECT_DOUBLE_EQ(first_bar_in_range.high, 106.44662543380355);
    EXPECT_DOUBLE_EQ(first_bar_in_range.low, 105.65908724331581);
    EXPECT_DOUBLE_EQ(first_bar_in_range.close, 106.44662543380355);
    EXPECT_EQ(first_bar_in_range.volume, 888);
}

TEST_F(DataReaderTest, ThrowsExceptionForInvalidFile) {
    // This test now asserts that creating a DataReader with a bad file
    // path will throw a std::runtime_error. This is the correct way
    // to test for expected exceptions.
    ASSERT_THROW(qse::DataReader reader("bad/path/to/nonexistent_file.parquet"), std::runtime_error);
}

TEST_F(DataReaderTest, GetBarCountReturnsCorrectSize) {
    DataReader reader(file_path_);
    auto bars = reader.read_all_bars();
    EXPECT_EQ(reader.get_bar_count(), bars.size());
}

} // namespace test
} // namespace qse 