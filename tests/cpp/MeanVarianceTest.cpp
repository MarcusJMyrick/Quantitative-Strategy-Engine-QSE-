// A5: mean-variance extension of PortfolioBuilder. The done-when pair:
// λ = 0 reproduces the legacy alpha-maximizing weights exactly, and growing
// λ tilts the portfolio toward minimum variance. The tilt test is designed
// to be analytically exact: with zero betas and a symmetric ±α universe the
// projections are no-ops and the optimum is w_i = α_i / (2γ + λσ_i²), so the
// low-vol/high-vol weight ratio must approach σ_h²/σ_l².

#include <gtest/gtest.h>
#include "qse/factor/PortfolioBuilder.h"

#include <cstdio>
#include <fstream>
#include <vector>

namespace {

const std::vector<std::string> kSymbols = {"A", "B", "C", "D"};

qse::PortfolioBuilder::OptimizationConfig precise_config(double lambda) {
    qse::PortfolioBuilder::OptimizationConfig config;
    config.risk_aversion = lambda;
    config.gross_cap = 10.0;       // loose: the cap must not bind
    config.max_iterations = 20000; // run to a sharp optimum
    config.convergence_tol = 1e-12;
    return config;
}

double variance_of(const std::vector<double>& weights, const std::vector<double>& sigmas) {
    double var = 0.0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        var += weights[i] * weights[i] * sigmas[i] * sigmas[i];
    }
    return var;
}

} // namespace

TEST(MeanVarianceTest, LambdaZeroReproducesLegacyWeightsExactly) {
    const std::vector<double> alphas = {0.10, 0.05, -0.05, -0.10};
    const std::vector<double> betas = {1.2, 0.8, 1.0, 0.9};
    const std::vector<double> sigmas = {0.30, 0.20, 0.25, 0.15};

    qse::PortfolioBuilder legacy; // default config: risk_aversion = 0
    auto legacy_result = legacy.optimize(alphas, betas, kSymbols);

    qse::PortfolioBuilder mean_variance;
    qse::PortfolioBuilder::OptimizationConfig config; // defaults, λ = 0
    mean_variance.set_config(config);
    auto mv_result = mean_variance.optimize(alphas, betas, sigmas, kSymbols);

    ASSERT_EQ(legacy_result.weights.size(), mv_result.weights.size());
    for (std::size_t i = 0; i < legacy_result.weights.size(); ++i) {
        EXPECT_NEAR(mv_result.weights[i], legacy_result.weights[i], 1e-12)
            << "weight " << i << " diverged at lambda=0";
    }
    EXPECT_NEAR(mv_result.objective_value, legacy_result.objective_value, 1e-12);
}

TEST(MeanVarianceTest, RisingLambdaTiltsTowardLowVolAssets) {
    // Symmetric ±α with zero betas: projections are no-ops, so the optimum
    // is w_i = α_i / (2γ + λσ_i²) in closed form
    const std::vector<double> alphas = {0.10, 0.10, -0.10, -0.10};
    const std::vector<double> betas = {0.0, 0.0, 0.0, 0.0};
    const std::vector<double> sigmas = {0.40, 0.10, 0.40, 0.10}; // vol ratio² = 16

    double prev_ratio = 0.0;
    double prev_variance = std::numeric_limits<double>::max();
    for (double lambda : {0.0, 1.0, 10.0, 200.0}) {
        qse::PortfolioBuilder builder;
        builder.set_config(precise_config(lambda));
        auto result = builder.optimize(alphas, betas, sigmas, kSymbols);

        // Low-vol asset B gains weight relative to high-vol asset A
        const double ratio = result.weights[1] / result.weights[0];
        EXPECT_GE(ratio, prev_ratio) << "tilt must be monotone in lambda (λ=" << lambda << ")";
        prev_ratio = ratio;

        // And total portfolio variance falls monotonically
        const double variance = variance_of(result.weights, sigmas);
        EXPECT_LT(variance, prev_variance) << "variance must fall with lambda (λ=" << lambda << ")";
        prev_variance = variance;

        if (lambda == 0.0) {
            EXPECT_NEAR(ratio, 1.0, 1e-6); // equal alphas, no risk term: equal weights
        }
        if (lambda == 200.0) {
            // Closed form: (2γ + λσ_h²)/(2γ + λσ_l²) = 32.02/2.02 = 15.85...
            EXPECT_NEAR(ratio, 15.85, 0.8);
        }
    }
}

TEST(MeanVarianceTest, MarketVarianceChannelPenalizesFactorExposure) {
    // All-long-beta book: with β = {1,1,-1,-1} and ±α weights, βᵀw is large,
    // so σ_m²ββᵀ bites. Beta projection is disabled via a huge tolerance so
    // the market-risk term (not the constraint) does the work
    const std::vector<double> alphas = {0.10, 0.10, -0.10, -0.10};
    const std::vector<double> betas = {1.0, 1.0, -1.0, -1.0};
    const std::vector<double> sigmas = {0.10, 0.10, 0.10, 0.10};

    auto run = [&](double market_variance) {
        qse::PortfolioBuilder builder;
        auto config = precise_config(10.0);
        config.beta_tolerance = 1e9; // skip the beta projection entirely
        config.market_variance = market_variance;
        builder.set_config(config);
        return builder.optimize(alphas, betas, sigmas, kSymbols);
    };

    auto no_market_risk = run(0.0);
    auto with_market_risk = run(1.0);
    EXPECT_LT(with_market_risk.gross_exposure, no_market_risk.gross_exposure)
        << "pricing factor risk must shrink the factor-exposed book";
}

TEST(MeanVarianceTest, YamlLoadsNewKeysAndOldConfigsStillWork) {
    // New keys present
    {
        std::ofstream ofs("mv_config.yaml");
        ofs << "portfolio_optimizer:\n"
               "    gamma: 0.05\n"
               "    gross_cap: 1.5\n"
               "    beta_target: 0.0\n"
               "    beta_tolerance: 1e-7\n"
               "    max_iterations: 500\n"
               "    convergence_tol: 1e-8\n"
               "    risk_aversion: 2.5\n"
               "    market_variance: 0.04\n";
    }
    qse::PortfolioBuilder builder;
    builder.load_config("mv_config.yaml");
    EXPECT_DOUBLE_EQ(builder.config().risk_aversion, 2.5);
    EXPECT_DOUBLE_EQ(builder.config().market_variance, 0.04);
    std::remove("mv_config.yaml");

    // Pre-A5 config without the new keys: defaults survive
    {
        std::ofstream ofs("legacy_config.yaml");
        ofs << "portfolio_optimizer:\n"
               "    gamma: 0.05\n"
               "    gross_cap: 1.5\n"
               "    beta_target: 0.0\n"
               "    beta_tolerance: 1e-7\n"
               "    max_iterations: 500\n"
               "    convergence_tol: 1e-8\n";
    }
    qse::PortfolioBuilder legacy;
    legacy.load_config("legacy_config.yaml");
    EXPECT_DOUBLE_EQ(legacy.config().risk_aversion, 0.0);
    EXPECT_DOUBLE_EQ(legacy.config().market_variance, 1.0);
    std::remove("legacy_config.yaml");
}
