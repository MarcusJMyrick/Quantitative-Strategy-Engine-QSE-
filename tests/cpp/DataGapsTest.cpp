// B1: the reader must tolerate malformed rows (count them, don't abort) and
// surface missing rows in the time grid via gap_count().

#include <gtest/gtest.h>
#include "qse/data/CSVDataReader.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

class DataGapsTest : public ::testing::Test {
protected:
    std::string path_;

    void write_file(const std::string& name, const std::string& content) {
        path_ = name;
        std::ofstream file(path_);
        file << content;
    }

    void TearDown() override {
        if (!path_.empty()) {
            std::remove(path_.c_str());
        }
    }
};

} // namespace

TEST_F(DataGapsTest, CleanBarFileReportsZero) {
    write_file("gaps_clean.csv", "timestamp,open,high,low,close,volume\n"
                                 "1000,10,11,9,10.5,100\n"
                                 "1060,10.5,11,10,10.8,120\n"
                                 "1120,10.8,11.2,10.5,11,90\n"
                                 "1180,11,11.5,10.9,11.2,80\n");
    qse::CSVDataReader reader(path_, "TEST");
    EXPECT_EQ(reader.read_all_bars().size(), 4u);
    EXPECT_EQ(reader.skipped_row_count(), 0u);
    EXPECT_EQ(reader.gap_count(), 0u);
}

TEST_F(DataGapsTest, MalformedBarRowsAreSkippedAndCounted) {
    // One row with an empty close field, one row with too few columns;
    // neither may abort the load
    write_file("gaps_malformed.csv", "timestamp,open,high,low,close,volume\n"
                                     "1000,10,11,9,10.5,100\n"
                                     "1060,10.5,11,10,,120\n"
                                     "1120,10.8\n"
                                     "1180,11,11.5,10.9,11.2,80\n");
    qse::CSVDataReader reader(path_, "TEST");
    EXPECT_EQ(reader.read_all_bars().size(), 2u);
    EXPECT_EQ(reader.skipped_row_count(), 2u);
}

TEST_F(DataGapsTest, MissingBarsInGridAreCounted) {
    // 60s grid with bars at t, t+60, t+120, then a jump to t+360:
    // t+180, t+240, t+300 are missing -> 3 gaps
    write_file("gaps_grid.csv", "timestamp,open,high,low,close,volume\n"
                                "1000,10,11,9,10.5,100\n"
                                "1060,10.5,11,10,10.8,120\n"
                                "1120,10.8,11.2,10.5,11,90\n"
                                "1360,11,11.5,10.9,11.2,80\n"
                                "1420,11.2,11.6,11,11.4,70\n");
    qse::CSVDataReader reader(path_, "TEST");
    EXPECT_EQ(reader.read_all_bars().size(), 5u);
    EXPECT_EQ(reader.skipped_row_count(), 0u);
    EXPECT_EQ(reader.gap_count(), 3u);
}

TEST_F(DataGapsTest, TickGridGapsAndBadRowsAreCounted) {
    // Legacy tick format on a 60s grid: one unparseable price (1060) and one
    // genuinely missing row (1360). The skipped row leaves a hole in the
    // grid, so both register as gaps: deltas {120,60,60,60,120}, median 60,
    // and each 120 delta contributes one missing row.
    write_file("gaps_ticks.csv", "timestamp,price,volume\n"
                                 "1000,100.0,500\n"
                                 "1060,not_a_price,500\n"
                                 "1120,100.4,500\n"
                                 "1180,100.5,500\n"
                                 "1240,100.6,500\n"
                                 "1300,100.8,500\n"
                                 "1420,100.9,500\n");
    qse::CSVDataReader reader(path_, "TEST");
    EXPECT_EQ(reader.read_all_ticks().size(), 6u);
    EXPECT_EQ(reader.skipped_row_count(), 1u);
    EXPECT_EQ(reader.gap_count(), 2u);
}
