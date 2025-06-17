#include <gtest/gtest.h>
#include "BarBuilder.h"
#include "Data.h" // For Tick and Bar structs
#include <vector>
#include <chrono>
#include <optional>

// Use the C++ standard library's chrono literals for cleaner time duration syntax (e.g., 60s)
using namespace std::chrono_literals;

// A test fixture class to set up a common environment for all BarBuilder tests.
class BarBuilderTest : public ::testing::Test {
protected:
    // Create a BarBuilder that will construct 1-minute (60-second) bars.
    qse::BarBuilder bar_builder_{60s};

    // A helper function to easily create Tick objects for our tests.
    qse::Tick create_tick(int seconds_offset, double price, long long volume = 100) {
        // Use a fixed starting point in time for consistent test results.
        auto start_time = std::chrono::system_clock::from_time_t(1609459200); // 2021-01-01 00:00:00 UTC
        return {start_time + std::chrono::seconds(seconds_offset), price, static_cast<qse::Volume>(volume)};
    }
};

// Test to ensure the BarBuilder correctly aggregates ticks that fall within a single bar interval.
TEST_F(BarBuilderTest, FormsSingleBarCorrectly) {
    // Arrange: Create a series of ticks that are all within the same 60-second window.
    auto tick1 = create_tick(10, 100.0); // The Open price
    auto tick2 = create_tick(20, 102.0); // This should become the High
    auto tick3 = create_tick(30, 99.0);  // This should become the Low
    auto tick4 = create_tick(40, 101.0); // This should become the Close

    // Act: Add the ticks to the builder.
    bar_builder_.add_tick(tick1);
    bar_builder_.add_tick(tick2);
    bar_builder_.add_tick(tick3);
    auto result = bar_builder_.add_tick(tick4);

    // Assert: Since all ticks are within the 60s interval, no bar should be completed yet.
    ASSERT_FALSE(result.has_value());
}

// Test to ensure the BarBuilder completes the old bar and returns it when a new tick
// arrives that belongs to the next time interval.
TEST_F(BarBuilderTest, CompletesBarOnNextIntervalTick) {
    // Arrange: Create four ticks for the first bar.
    bar_builder_.add_tick(create_tick(10, 100.0)); // Open
    bar_builder_.add_tick(create_tick(20, 102.0)); // High
    bar_builder_.add_tick(create_tick(30, 99.0));  // Low
    bar_builder_.add_tick(create_tick(40, 101.0)); // Close
    
    // This tick is at second 70, which is in the *next* 60-second interval.
    // This tick should trigger the completion of the first bar.
    auto closing_tick = create_tick(70, 105.0); 

    // Act: Add the final tick.
    auto result = bar_builder_.add_tick(closing_tick);

    // Assert: A bar should be complete now.
    ASSERT_TRUE(result.has_value());
    
    // Verify the completed bar's values are correct.
    qse::Bar completed_bar = *result;
    EXPECT_DOUBLE_EQ(completed_bar.open, 100.0);
    EXPECT_DOUBLE_EQ(completed_bar.high, 102.0);
    EXPECT_DOUBLE_EQ(completed_bar.low, 99.0);
    EXPECT_DOUBLE_EQ(completed_bar.close, 101.0);
    EXPECT_EQ(completed_bar.volume, 400); // 100 volume for each of the 4 ticks
} 