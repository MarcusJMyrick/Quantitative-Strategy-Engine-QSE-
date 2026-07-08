// QR1.2 — VPIN (Easley-López de Prado-O'Hara). Done-when: a gtest computes VPIN
// on a synthetic volume series with a known buy/sell split within tolerance.

#include <gtest/gtest.h>

#include "qse/microstructure/VPINCalculator.h"

#include <cmath>

using qse::VPINCalculator;

// --- bulk-volume classification primitives ---

TEST(VPINTest, NormalCdfKnownValues) {
    EXPECT_NEAR(VPINCalculator::normal_cdf(0.0), 0.5, 1e-12);
    EXPECT_NEAR(VPINCalculator::normal_cdf(1.0), 0.8413447, 1e-6);
    EXPECT_NEAR(VPINCalculator::normal_cdf(-1.0), 0.1586553, 1e-6);
    EXPECT_NEAR(VPINCalculator::normal_cdf(8.0), 1.0, 1e-9); // deep tail
}

TEST(VPINTest, BuyFractionAndImbalance) {
    // no price change → balanced → imbalance 0
    EXPECT_NEAR(VPINCalculator::buy_fraction(0.0, 1.0), 0.5, 1e-12);
    EXPECT_NEAR(VPINCalculator::order_imbalance(0.0, 1.0), 0.0, 1e-12);
    // one standard deviation up → 84% buy
    EXPECT_NEAR(VPINCalculator::buy_fraction(1.0, 1.0), 0.8413447, 1e-6);
    // strongly one-sided → imbalance ≈ 1
    EXPECT_NEAR(VPINCalculator::order_imbalance(6.0, 1.0), 1.0, 1e-6);
    // σ ≤ 0 carries no information → 0.5 buy, 0 imbalance
    EXPECT_DOUBLE_EQ(VPINCalculator::buy_fraction(3.0, 0.0), 0.5);
    EXPECT_DOUBLE_EQ(VPINCalculator::order_imbalance(3.0, 0.0), 0.0);
}

// --- equal-volume bucketing ---

TEST(VPINTest, BucketingCountsAndSplitsTrades) {
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/5);
    v.on_trade(100.0, 250); // 2 full buckets + 50 partial
    EXPECT_EQ(v.total_buckets(), 2u);
    v.on_trade(100.0, 50); // completes the 3rd
    EXPECT_EQ(v.total_buckets(), 3u);
    v.on_trade(100.0, 100); // exactly one more
    EXPECT_EQ(v.total_buckets(), 4u);
}

TEST(VPINTest, NotReadyUntilEnoughBuckets) {
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/3, /*fixed_sigma=*/1.0);
    for (double px : {100.0, 101.0, 100.0}) { // 3 buckets → only 2 ΔP
        v.on_trade(px, 100);
    }
    EXPECT_FALSE(v.ready());
    EXPECT_TRUE(std::isnan(v.vpin()));
    v.on_trade(101.0, 100); // 4th bucket → 3rd ΔP → ready
    EXPECT_TRUE(v.ready());
}

// --- the done-when: known buy/sell split (fixed σ makes it exact) ---

TEST(VPINTest, KnownSplitWithFixedSigma) {
    // Alternating closes ±1 with σ = 1 → every ΔP is ±1σ, so each bucket's
    // imbalance is |2Φ(1) − 1| exactly, and VPIN equals that.
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/4, /*fixed_sigma=*/1.0);
    for (double px : {100.0, 101.0, 100.0, 101.0, 100.0}) { // 5 buckets → 4 ΔP of ±1
        v.on_trade(px, 100);
    }
    ASSERT_TRUE(v.ready());
    const double expected = std::abs(2.0 * VPINCalculator::normal_cdf(1.0) - 1.0); // ≈ 0.6827
    EXPECT_NEAR(v.vpin(), expected, 1e-9);
}

TEST(VPINTest, BalancedFlowIsLowVpin) {
    // flat prices → ΔP = 0 → 50/50 split → VPIN = 0
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/5, /*fixed_sigma=*/1.0);
    for (int i = 0; i < 8; ++i) {
        v.on_trade(100.0, 100);
    }
    ASSERT_TRUE(v.ready());
    EXPECT_NEAR(v.vpin(), 0.0, 1e-12);
}

TEST(VPINTest, OneSidedFlowIsHighVpin) {
    // steadily rising price, each step 5σ → almost all buy → VPIN ≈ 1
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/5, /*fixed_sigma=*/1.0);
    double px = 100.0;
    for (int i = 0; i < 8; ++i) {
        v.on_trade(px, 100);
        px += 5.0;
    }
    ASSERT_TRUE(v.ready());
    EXPECT_GT(v.vpin(), 0.99);
}

// --- estimated σ (the real, causal path) ---

TEST(VPINTest, EstimatedSigmaRecoversAlternatingSplit) {
    // No fixed σ: it is estimated as the expanding std of ΔP. With alternating
    // ±d closes, σ → d, so VPIN converges to |2Φ(1) − 1| within tolerance.
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/20); // estimate σ
    double px = 100.0;
    for (int i = 0; i < 60; ++i) {
        v.on_trade(px, 100);
        px = (i % 2 == 0) ? px + 2.0 : px - 2.0; // ΔP alternates +2 / −2 → σ ≈ 2
    }
    ASSERT_TRUE(v.ready());
    EXPECT_NEAR(v.current_sigma(), 2.0, 0.1);
    const double expected = std::abs(2.0 * VPINCalculator::normal_cdf(1.0) - 1.0);
    EXPECT_NEAR(v.vpin(), expected, 0.03);
}

TEST(VPINTest, ResetClearsState) {
    VPINCalculator v(/*bucket_volume=*/100, /*num_buckets=*/3, /*fixed_sigma=*/1.0);
    for (double px : {100.0, 101.0, 100.0, 101.0}) {
        v.on_trade(px, 100);
    }
    v.reset();
    EXPECT_EQ(v.total_buckets(), 0u);
    EXPECT_FALSE(v.ready());
    EXPECT_TRUE(std::isnan(v.vpin()));
}
