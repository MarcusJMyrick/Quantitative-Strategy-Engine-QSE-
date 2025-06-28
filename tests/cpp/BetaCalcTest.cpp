#include <gtest/gtest.h>
#include "qse/factor/RiskModel.h"
#include <cmath>
#include <random>

using namespace qse;

TEST(BetaCalcTest, Static60) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // synthetic CAPM path: identical returns => beta ~1
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60; cfg.apply_shrink=false;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    ASSERT_FALSE(std::isnan(beta.back()));
    EXPECT_NEAR(beta.back(), 1.0, 1e-2);
}

TEST(BetaCalcTest, Shrinkage) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset beta true ~1.5 (asset_ret = 1.5 * market_ret)
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1)*0.01;
        asset_ret[i]  = 1.5 * market_ret[i];
    }
    RiskModel::Config cfg; cfg.window=60; cfg.min_obs=60; cfg.apply_shrink=true; cfg.lambda=0.5;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    double b = beta.back();
    // shrunk towards 1: expected ~ (0.5*1 + 0.5*1.5)=1.25
    EXPECT_NEAR(b, 1.25, 0.05);
}

TEST(RiskModelTest, ResidSigma) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset beta 1, plus noise variance 0.0001
    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0, 0.01);
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1)*0.01;
        asset_ret[i]  = market_ret[i] + nd(rng);
    }
    RiskModel::Config cfg; cfg.window=60; cfg.min_obs=60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    auto sigma = rm.rolling_resid_sigma(asset_ret, market_ret, beta);
    ASSERT_FALSE(std::isnan(sigma.back()));
    // sigma should be close to 0.01 (std dev of noise)
    EXPECT_NEAR(sigma.back(), 0.01, 0.005);
}

// Edge Cases & Error Handling
TEST(BetaCalcTest, InsufficientObservations) {
    size_t n = 30; // less than min_obs=60
    std::vector<double> market_ret(n), asset_ret(n);
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    // All betas should be NaN due to insufficient observations
    for (double b : beta) {
        EXPECT_TRUE(std::isnan(b));
    }
}

TEST(BetaCalcTest, ZeroMarketVariance) {
    size_t n = 120;
    std::vector<double> market_ret(n, 0.0), asset_ret(n);
    for (size_t i = 0; i < n; ++i) {
        asset_ret[i] = std::sin(i*0.1) * 0.01;
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    // Should handle division by zero gracefully
    for (double b : beta) {
        EXPECT_TRUE(std::isnan(b));
    }
}

TEST(BetaCalcTest, NegativeBeta) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset_ret = -0.5 * market_ret (negative correlation)
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = -0.5 * market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    ASSERT_FALSE(std::isnan(beta.back()));
    EXPECT_NEAR(beta.back(), -0.5, 0.05);
}

TEST(BetaCalcTest, HighBetaStock) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset_ret = 2.0 * market_ret (high beta)
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = 2.0 * market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    ASSERT_FALSE(std::isnan(beta.back()));
    EXPECT_NEAR(beta.back(), 2.0, 0.05);
}

TEST(BetaCalcTest, LowBetaStock) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset_ret = 0.3 * market_ret (low beta)
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = 0.3 * market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    ASSERT_FALSE(std::isnan(beta.back()));
    EXPECT_NEAR(beta.back(), 0.3, 0.05);
}

TEST(BetaCalcTest, NonStationaryBeta) {
    size_t n = 200;
    std::vector<double> market_ret(n), asset_ret(n);
    // Beta changes from 0.5 to 1.5 halfway through
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        double beta_t = (i < n/2) ? 0.5 : 1.5;
        asset_ret[i]  = beta_t * market_ret[i];
    }
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    
    // Check that beta adapts to the change
    EXPECT_NEAR(beta[n/2 - 1], 0.5, 0.1);  // Before change
    EXPECT_NEAR(beta.back(), 1.5, 0.1);     // After change
}

TEST(BetaCalcTest, ShrinkageBounds) {
    size_t n = 120;
    std::vector<double> market_ret(n), asset_ret(n);
    // asset_ret = 2.0 * market_ret (high beta)
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        asset_ret[i]  = 2.0 * market_ret[i];
    }
    
    // Test different shrinkage levels
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60; cfg.apply_shrink = true;
    
    // No shrinkage
    cfg.lambda = 0.0;
    RiskModel rm1(cfg);
    auto beta1 = rm1.rolling_beta(asset_ret, market_ret);
    EXPECT_NEAR(beta1.back(), 2.0, 0.05);
    
    // Full shrinkage
    cfg.lambda = 1.0;
    RiskModel rm2(cfg);
    auto beta2 = rm2.rolling_beta(asset_ret, market_ret);
    EXPECT_NEAR(beta2.back(), 1.0, 0.05);
    
    // Partial shrinkage
    cfg.lambda = 0.3;
    RiskModel rm3(cfg);
    auto beta3 = rm3.rolling_beta(asset_ret, market_ret);
    double expected = 0.3 * 1.0 + 0.7 * 2.0;  // 1.7
    EXPECT_NEAR(beta3.back(), expected, 0.05);
}

TEST(RiskModelTest, ResidSigmaWithChangingBeta) {
    size_t n = 200;
    std::vector<double> market_ret(n), asset_ret(n);
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 0.02);
    
    // Beta changes from 0.5 to 1.5, plus constant noise
    for (size_t i = 0; i < n; ++i) {
        market_ret[i] = std::sin(i*0.1) * 0.01;
        double beta_t = (i < n/2) ? 0.5 : 1.5;
        asset_ret[i]  = beta_t * market_ret[i] + noise(rng);
    }
    
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    auto sigma = rm.rolling_resid_sigma(asset_ret, market_ret, beta);
    
    // Residual sigma should be close to noise std dev
    EXPECT_NEAR(sigma.back(), 0.02, 0.01);
}

TEST(RiskModelTest, EmptyInput) {
    std::vector<double> empty_ret;
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    
    auto beta = rm.rolling_beta(empty_ret, empty_ret);
    auto sigma = rm.rolling_resid_sigma(empty_ret, empty_ret, beta);
    
    EXPECT_TRUE(beta.empty());
    EXPECT_TRUE(sigma.empty());
}

TEST(RiskModelTest, SingleObservation) {
    std::vector<double> market_ret{0.01}, asset_ret{0.02};
    RiskModel::Config cfg; cfg.window = 60; cfg.min_obs = 60;
    RiskModel rm(cfg);
    
    auto beta = rm.rolling_beta(asset_ret, market_ret);
    EXPECT_EQ(beta.size(), 1);
    EXPECT_TRUE(std::isnan(beta[0]));  // Insufficient observations
} 