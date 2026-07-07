#include <gtest/gtest.h>
#include "qse/strategy/WeightsLoader.h"
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace qse;

class WeightsLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir_ = "test_weights";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    void create_test_file(const std::string& filename, const std::string& content) {
        std::ofstream file(test_dir_ + "/" + filename);
        file << content;
        file.close();
    }

    std::string test_dir_;
};

TEST_F(WeightsLoaderTest, LoadsCorrectRows) {
    // Create a test weights file
    std::string content = "symbol,weight\n"
                          "AAPL,0.15\n"
                          "GOOG,-0.10\n"
                          "MSFT,0.08\n"
                          "TSLA,-0.05\n";

    create_test_file("weights_20241215.csv", content);

    // Create timestamp for 2024-12-15
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11; // December (0-based)
    tm.tm_mday = 15;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Load weights
    auto weights = WeightsLoader::load_daily_weights(test_dir_, timestamp);

    // Verify weights were loaded correctly
    ASSERT_TRUE(weights.has_value());
    EXPECT_EQ(weights->size(), 4);

    EXPECT_DOUBLE_EQ((*weights)["AAPL"], 0.15);
    EXPECT_DOUBLE_EQ((*weights)["GOOG"], -0.10);
    EXPECT_DOUBLE_EQ((*weights)["MSFT"], 0.08);
    EXPECT_DOUBLE_EQ((*weights)["TSLA"], -0.05);
}

TEST_F(WeightsLoaderTest, HandlesMissingFile) {
    // Create timestamp for a date with no weights file
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11;  // December
    tm.tm_mday = 16; // Different date
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Try to load weights for non-existent file
    auto weights = WeightsLoader::load_daily_weights(test_dir_, timestamp);

    // Should return nullopt, not throw
    EXPECT_FALSE(weights.has_value());
}

TEST_F(WeightsLoaderTest, GeneratesCorrectFilename) {
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11; // December
    tm.tm_mday = 15;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::string filename = WeightsLoader::generate_filename(test_dir_, timestamp);
    std::string expected = test_dir_ + "/weights_20241215.csv";

    EXPECT_EQ(filename, expected);
}

TEST_F(WeightsLoaderTest, HandlesInvalidCSV) {
    // Create a malformed CSV file
    std::string content = "symbol,weight\n"
                          "AAPL,0.15\n"
                          "GOOG,invalid_weight\n" // Invalid weight
                          "MSFT,0.08\n";

    create_test_file("weights_20241215.csv", content);

    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 11;
    tm.tm_mday = 15;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Should still load valid rows and skip invalid ones
    auto weights = WeightsLoader::load_daily_weights(test_dir_, timestamp);

    ASSERT_TRUE(weights.has_value());
    EXPECT_EQ(weights->size(), 2); // Only AAPL and MSFT should be loaded

    EXPECT_DOUBLE_EQ((*weights)["AAPL"], 0.15);
    EXPECT_DOUBLE_EQ((*weights)["MSFT"], 0.08);
    EXPECT_EQ(weights->count("GOOG"), 0); // GOOG should be skipped
}
// QR4.5: a dollar-neutral book emitted by scripts/research/statarb/signals.py
// (every universe name written, inactive at 0.0, net ~ 0) must load unchanged
// through the same loader the engine uses. This locks the Python->C++ handoff.
TEST_F(WeightsLoaderTest, LoadsDollarNeutralStatArbBook) {
    // Exactly the format signals.py writes: header + symbol,weight for every
    // name, longs/shorts equal-weighted to +/- gross/2, inactive names at 0.
    std::string content = "symbol,weight\n"
                          "AAPL,0.25\n"
                          "MSFT,0.25\n"
                          "GOOG,-0.5\n"
                          "AMZN,0\n"; // exited/inactive -> engine targets flat

    create_test_file("weights_20241216.csv", content);

    auto weights = WeightsLoader::load_weights_from_file(test_dir_ + "/weights_20241216.csv");
    ASSERT_TRUE(weights.has_value());
    EXPECT_EQ(weights->size(), 4); // all names present, including the zero

    double net = 0.0, gross = 0.0;
    for (const auto& [sym, w] : *weights) {
        net += w;
        gross += std::abs(w);
    }
    EXPECT_NEAR(net, 0.0, 1e-9);               // dollar-neutral
    EXPECT_NEAR(gross, 1.0, 1e-9);             // gross cap
    EXPECT_DOUBLE_EQ((*weights)["AMZN"], 0.0); // inactive name loads as a flat target
}
