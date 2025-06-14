#include <gtest/gtest.h>
#include "../src/DataReader.h"
#include <string>
#include <vector>
#include <chrono>

// This macro is provided by CMake to locate the test data directory
#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

// Test fixture to ensure the test data file exists
class DataReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Construct an absolute path to the test parquet file
        file_path = std::string(TEST_DATA_DIR) + "/test_bars.parquet";
    }

    std::string file_path;
};

TEST_F(DataReaderTest, CanReadParquetFile) {
    // 1. Arrange
    qse::DataReader reader(file_path);

    // 2. Act
    std::vector<qse::Bar> bars = reader.read_all_bars();

    // 3. Assert
    ASSERT_EQ(bars.size(), 2);

    // Check the first bar
    EXPECT_DOUBLE_EQ(bars[0].open, 100.0);
    EXPECT_DOUBLE_EQ(bars[0].high, 102.5);
    EXPECT_DOUBLE_EQ(bars[0].low, 99.5);
    EXPECT_DOUBLE_EQ(bars[0].close, 101.2);
    EXPECT_EQ(bars[0].volume, 1000);

    // Check the second bar
    EXPECT_DOUBLE_EQ(bars[1].open, 101.0);
    EXPECT_DOUBLE_EQ(bars[1].high, 102.8);
    EXPECT_DOUBLE_EQ(bars[1].low, 100.5);
    EXPECT_DOUBLE_EQ(bars[1].close, 101.9);
    EXPECT_EQ(bars[1].volume, 1200);
}

TEST_F(DataReaderTest, CanReadTimeRange) {
    // 1. Arrange
    qse::DataReader reader(file_path);
    
    // Create time range that includes only the first bar
    auto start_time = std::chrono::system_clock::from_time_t(
        std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() - std::chrono::hours(24)
        )
    );
    auto end_time = std::chrono::system_clock::from_time_t(
        std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() + std::chrono::hours(24)
        )
    );

    // 2. Act
    std::vector<qse::Bar> bars = reader.read_bars_in_range(start_time, end_time);

    // 3. Assert
    ASSERT_EQ(bars.size(), 2);  // Both bars should be within this range
}

TEST_F(DataReaderTest, ReturnsEmptyForInvalidFile) {
    // 1. Arrange
    qse::DataReader reader("nonexistent_file.parquet");

    // 2. Act
    std::vector<qse::Bar> bars = reader.read_all_bars();

    // 3. Assert
    EXPECT_TRUE(bars.empty());
}

TEST_F(DataReaderTest, GetBarCountReturnsCorrectSize) {
    // 1. Arrange
    qse::DataReader reader(file_path);

    // 2. Act
    size_t count = reader.get_bar_count();

    // 3. Assert
    EXPECT_EQ(count, 2);
} 