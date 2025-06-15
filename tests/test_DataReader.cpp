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
    // Define the duration in nanoseconds first
    auto start_ns = std::chrono::nanoseconds(1704110400000000000); // 2024-01-01 12:00:00 UTC
    auto end_ns = std::chrono::nanoseconds(1704153600000000000);   // 2024-01-02 00:00:00 UTC

    // Explicitly cast the duration to what system_clock expects
    Timestamp start_time = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(start_ns)
    );
    Timestamp end_time = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(end_ns)
    );

    // Create a DataReader instance with our test file
    DataReader reader(file_path_);

    // Read the bars in our specified range
    std::vector<Bar> bars = reader.read_bars_in_range(start_time, end_time);

    // Verify we got the expected number of bars
    EXPECT_EQ(bars.size(), 100);

    // Verify the first bar's data
    EXPECT_EQ(bars[0].timestamp, start_time);
    EXPECT_DOUBLE_EQ(bars[0].open, 100.0);
    EXPECT_DOUBLE_EQ(bars[0].high, 102.0);
    EXPECT_DOUBLE_EQ(bars[0].low, 98.0);
    EXPECT_DOUBLE_EQ(bars[0].close, 101.0);
    EXPECT_EQ(bars[0].volume, 1000);
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