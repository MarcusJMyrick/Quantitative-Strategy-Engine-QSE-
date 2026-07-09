// QR1.3 — toxicity filter: the VPIN+OFI decision policy, and OrderManager
// reading the running toxicity state from its tick stream. (The A/B slippage
// audit that decides whether the filter earns its place lives in the
// toxicity_audit tool + scripts/analysis/toxicity_audit.py.)

#include <gtest/gtest.h>

#include "qse/data/Data.h"
#include "qse/microstructure/ToxicityFilter.h"
#include "qse/order/OrderManager.h"

#include <cstdio>

using qse::ExecAction;
using qse::ToxicityFilter;

// --- the decision policy ---

TEST(ToxicityFilterTest, BenignFlowAlwaysCrosses) {
    ToxicityFilter f(/*vpin_threshold=*/0.5);
    EXPECT_EQ(f.decide(/*is_buy=*/true, /*vpin=*/0.2, /*ofi=*/-100.0), ExecAction::Cross);
    EXPECT_EQ(f.decide(/*is_buy=*/false, /*vpin=*/0.2, /*ofi=*/100.0), ExecAction::Cross);
}

TEST(ToxicityFilterTest, ToxicAndFavorableRestsPassive) {
    ToxicityFilter f(0.5);
    // a buy wants net selling pressure (ofi < 0) to come hit its resting bid
    EXPECT_EQ(f.decide(/*is_buy=*/true, 0.8, -50.0), ExecAction::RestPassive);
    // a sell wants net buying (ofi > 0)
    EXPECT_EQ(f.decide(/*is_buy=*/false, 0.8, 50.0), ExecAction::RestPassive);
}

TEST(ToxicityFilterTest, ToxicButAdverseDirectionStillCrosses) {
    ToxicityFilter f(0.5);
    // buying into net *buying* toxic flow: resting passive would just miss the
    // fill and fall back worse, so cross
    EXPECT_EQ(f.decide(/*is_buy=*/true, 0.8, +50.0), ExecAction::Cross);
    EXPECT_EQ(f.decide(/*is_buy=*/false, 0.8, -50.0), ExecAction::Cross);
}

TEST(ToxicityFilterTest, ThresholdIsStrict) {
    ToxicityFilter f(0.5);
    EXPECT_EQ(f.decide(true, 0.50, -1.0), ExecAction::Cross); // not above threshold
    EXPECT_EQ(f.decide(true, 0.5001, -1.0), ExecAction::RestPassive);
}

// --- OrderManager reads the running toxicity state ---

namespace {
qse::Tick trade(qse::Price price, qse::Volume volume) {
    qse::Tick t{};
    t.symbol = "AAPL";
    t.price = price;
    t.volume = volume;
    t.bid = price - 0.01;
    t.ask = price + 0.01;
    t.bid_size = volume;
    t.ask_size = volume;
    return t;
}
} // namespace

TEST(ToxicityFilterTest, OrderManagerFlagsOneSidedFlowToxic) {
    qse::OrderManager om(1'000'000.0, "tox_equity.csv", "tox_tradelog.csv");
    om.enable_toxicity_filter(/*bucket_volume=*/100, /*num_buckets=*/5, /*threshold=*/0.4);
    EXPECT_TRUE(om.toxicity_filter_enabled());

    // steadily rising price with varying positive increments → one-sided (buy)
    // flow → VPIN → ~1, and OFI positive (demand stepping up)
    double px = 100.0;
    const double steps[] = {2.0, 3.0, 2.0, 4.0, 3.0, 2.0, 3.0, 4.0};
    for (double s : steps) {
        px += s;
        om.process_tick(trade(px, 100));
    }
    EXPECT_TRUE(om.is_toxic());
    EXPECT_GT(om.current_vpin(), 0.4);
    EXPECT_GT(om.current_ofi(), 0.0); // net buying pressure

    std::remove("tox_equity.csv");
    std::remove("tox_tradelog.csv");
}

TEST(ToxicityFilterTest, OrderManagerFlatFlowIsNotToxic) {
    qse::OrderManager om(1'000'000.0, "tox2_equity.csv", "tox2_tradelog.csv");
    om.enable_toxicity_filter(/*bucket_volume=*/100, /*num_buckets=*/5, /*threshold=*/0.4);

    for (int i = 0; i < 8; ++i) {
        om.process_tick(trade(100.0, 100)); // flat price → ΔP = 0 → VPIN → 0
    }
    EXPECT_FALSE(om.is_toxic());
    EXPECT_NEAR(om.current_vpin(), 0.0, 1e-9);

    std::remove("tox2_equity.csv");
    std::remove("tox2_tradelog.csv");
}

TEST(ToxicityFilterTest, DisabledByDefault) {
    qse::OrderManager om(1'000'000.0, "tox3_equity.csv", "tox3_tradelog.csv");
    EXPECT_FALSE(om.toxicity_filter_enabled());
    EXPECT_FALSE(om.is_toxic());
    om.process_tick(trade(100.0, 100)); // must not crash without a filter
    std::remove("tox3_equity.csv");
    std::remove("tox3_tradelog.csv");
}
