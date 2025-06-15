#include <gtest/gtest.h>
#include "CSVDataReader.h"
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace qse {
namespace test {

// This macro is provided by CMake to locate the test data directory
#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

class DataReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data directory if it doesn't exist
        std::filesystem::create_directories("test_data");
        
        // Create a test CSV file
        file_path_ = "test_data/test_bars.csv";
        std::ofstream file(file_path_);
        file << "timestamp,open,high,low,close,volume\n";
        file << "1704067200,100.0,101.0,99.0,100.5,1000\n";
        file << "1704153600,100.5,102.0,100.0,101.5,2000\n";
        file << "1704240000,101.5,103.0,101.0,102.5,3000\n";
    }

    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(file_path_);
    }

    std::string file_path_;
};

TEST_F(DataReaderTest, CanReadCSVFile) {
    CSVDataReader reader(file_path_);
    EXPECT_EQ(reader.get_bar_count(), 3);
}

TEST_F(DataReaderTest, CanReadTimeRange) {
    CSVDataReader reader(file_path_);
    auto bars = reader.read_bars_in_range(1704067200, 1704153600);
    EXPECT_EQ(bars.size(), 2);
}

TEST_F(DataReaderTest, ThrowsExceptionForInvalidFile) {
    ASSERT_THROW(qse::CSVDataReader reader("bad/path/to/nonexistent_file.csv"), std::runtime_error);
}

TEST_F(DataReaderTest, GetBarCountReturnsCorrectSize) {
    CSVDataReader reader(file_path_);
    EXPECT_EQ(reader.get_bar_count(), 3);
}

} // namespace test
} // namespace qse 