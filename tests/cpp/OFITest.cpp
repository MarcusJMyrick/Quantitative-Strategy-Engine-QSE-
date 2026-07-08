// QR1.1 — Order Flow Imbalance (Cont-Kukanov-Stoikov) on L1 quotes. Done-when:
// a gtest reproduces OFI on a hand-built tick sequence with known level changes.

#include <gtest/gtest.h>

#include "qse/data/Data.h"
#include "qse/microstructure/OFICalculator.h"

namespace {

// L1 snapshot; trade price/volume are irrelevant to OFI, set to the mid.
qse::Tick quote(qse::Price bid, qse::Volume bid_size, qse::Price ask, qse::Volume ask_size) {
    qse::Tick t{};
    t.symbol = "TEST";
    t.bid = bid;
    t.bid_size = bid_size;
    t.ask = ask;
    t.ask_size = ask_size;
    t.price = (bid + ask) / 2.0;
    return t;
}

} // namespace

// --- the three bid cases (ask held flat with unchanged size => 0 ask term) ---

TEST(OFITest, BidUptickAddsNewSize) {
    // bid 100→101: demand stepped up, contribute +new bid size
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 8, 101.0, 5, 101.0, 8), 5.0);
}

TEST(OFITest, BidDowntickSubtractsPriorSize) {
    // bid 100→99: demand withdrawn, contribute −prior bid size
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 8, 99.0, 7, 101.0, 8), -10.0);
}

TEST(OFITest, BidFlatIsSizeDelta) {
    // bid flat, size 10→14: +4; and a shrinking size must go negative (no
    // unsigned underflow on the uint64 Volume type)
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 8, 100.0, 14, 101.0, 8), 4.0);
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 8, 100.0, 3, 101.0, 8), -7.0);
}

// --- the three ask cases (bid held flat with unchanged size => 0 bid term) ---

TEST(OFITest, AskUptickAddsPriorAskSize) {
    // ask 101→102: supply withdrawn (bullish), contribute +prior ask size
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 9, 100.0, 10, 102.0, 4), 9.0);
}

TEST(OFITest, AskDowntickSubtractsNewSize) {
    // ask 101→100: new supply (bearish), contribute −new ask size
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 9, 100.0, 10, 100.0, 6), -6.0);
}

TEST(OFITest, AskFlatIsNegativeSizeDelta) {
    // ask flat, size 9→4: supply reduced at the touch (bullish) => +5
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 9, 100.0, 10, 101.0, 4), 5.0);
}

TEST(OFITest, CombinedBullishEvent) {
    // bid up (+6) and ask up (+prior ask 9) => strongly positive
    EXPECT_DOUBLE_EQ(qse::OFICalculator::event_ofi(100.0, 10, 101.0, 9, 101.0, 6, 102.0, 4), 15.0);
}

// --- stateful driver ---

TEST(OFITest, FirstSnapshotHasNoEvent) {
    qse::OFICalculator ofi;
    EXPECT_DOUBLE_EQ(ofi.update(quote(100.0, 10, 101.0, 10)), 0.0);
    EXPECT_TRUE(ofi.has_prev());
    EXPECT_EQ(ofi.count(), 0u);
    EXPECT_DOUBLE_EQ(ofi.rolling_ofi(), 0.0);
}

// The done-when: a hand-built sequence with known level changes reproduces the
// per-event OFI and the running sum.
TEST(OFITest, HandBuiltSequenceReproducesKnownOFI) {
    qse::OFICalculator ofi(/*window=*/100);
    ofi.update(quote(100.0, 10, 101.0, 10)); // seed, no event

    EXPECT_DOUBLE_EQ(ofi.update(quote(100.0, 15, 101.0, 10)), 5.0); // bid size +5
    EXPECT_DOUBLE_EQ(ofi.update(quote(101.0, 8, 101.0, 10)), 8.0);  // bid uptick → +8
    EXPECT_DOUBLE_EQ(ofi.update(quote(101.0, 8, 100.0, 6)), -6.0);  // ask downtick → −6
    EXPECT_DOUBLE_EQ(ofi.update(quote(100.0, 8, 100.0, 6)), -8.0); // bid downtick → −8 (prior 8)

    EXPECT_EQ(ofi.count(), 4u);
    EXPECT_DOUBLE_EQ(ofi.rolling_ofi(), 5.0 + 8.0 - 6.0 - 8.0); // −1
    EXPECT_DOUBLE_EQ(ofi.last_event(), -8.0);
}

TEST(OFITest, RollingWindowEvictsOldest) {
    qse::OFICalculator ofi(/*window=*/2);
    ofi.update(quote(100.0, 10, 101.0, 10)); // seed
    ofi.update(quote(100.0, 13, 101.0, 10)); // +3
    ofi.update(quote(100.0, 16, 101.0, 10)); // +3  (window now [+3,+3])
    EXPECT_DOUBLE_EQ(ofi.rolling_ofi(), 6.0);
    ofi.update(quote(100.0, 20, 101.0, 10)); // +4  (evicts the first +3)
    EXPECT_EQ(ofi.count(), 2u);
    EXPECT_DOUBLE_EQ(ofi.rolling_ofi(), 3.0 + 4.0); // 7
}

TEST(OFITest, ResetClearsState) {
    qse::OFICalculator ofi;
    ofi.update(quote(100.0, 10, 101.0, 10));
    ofi.update(quote(100.0, 15, 101.0, 10));
    ofi.reset();
    EXPECT_FALSE(ofi.has_prev());
    EXPECT_EQ(ofi.count(), 0u);
    EXPECT_DOUBLE_EQ(ofi.rolling_ofi(), 0.0);
    EXPECT_DOUBLE_EQ(ofi.update(quote(100.0, 10, 101.0, 10)), 0.0); // fresh seed
}
