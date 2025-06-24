#include "gtest/gtest.h"
#include "qse/data/BarBuilder.h"
#include "qse/data/Data.h"

using namespace qse;

TEST(BarBuilderTest, AggregatesTicksIntoBars) {
    // 1-second bars
    BarBuilder builder(std::chrono::seconds(1));

    // Scrambled tick input (ms, price, volume)
    std::vector<Tick> ticks = {
        {"TEST", from_unix_ms(2500), 11.0, 0.0, 0.0, 3},
        {"TEST", from_unix_ms(1000), 10.0, 0.0, 0.0, 1},
        {"TEST", from_unix_ms(1500), 12.0, 0.0, 0.0, 2},
    };

    std::vector<Bar> completed;

    // 1) On each tick, capture _only_ the bar that add_tick() returns
    for (auto const& tk : ticks) {
        if (auto b = builder.add_tick(tk)) {
            completed.push_back(*b);
        }
    }

    // 2) Finally, flush the one remaining bar
    if (auto b = builder.flush()) {
        completed.push_back(*b);
    }

    // Sort by start timestamp so we assert chronologically:
    std::sort(completed.begin(), completed.end(),
        [](auto const &a, auto const &b){
            return to_unix_ms(a.timestamp) < to_unix_ms(b.timestamp);
        }
    );

    // Now we should have exactly two bars:
    ASSERT_EQ(completed.size(), 2u);

    // Bar 1: [1000,2000)
    auto &bar1 = completed[0];
    EXPECT_EQ(to_unix_ms(bar1.timestamp), 1000LL);
    EXPECT_DOUBLE_EQ(bar1.open,  10.0);
    EXPECT_DOUBLE_EQ(bar1.high,  12.0);
    EXPECT_DOUBLE_EQ(bar1.low,   10.0);
    EXPECT_DOUBLE_EQ(bar1.close, 12.0);
    EXPECT_EQ(bar1.volume,       3);

    // Bar 2: [2000,3000)
    auto &bar2 = completed[1];
    EXPECT_EQ(to_unix_ms(bar2.timestamp), 2000LL);
    EXPECT_DOUBLE_EQ(bar2.open,  11.0);
    EXPECT_DOUBLE_EQ(bar2.high,  11.0);
    EXPECT_DOUBLE_EQ(bar2.low,   11.0);
    EXPECT_DOUBLE_EQ(bar2.close, 11.0);
    EXPECT_EQ(bar2.volume,       3);
} 