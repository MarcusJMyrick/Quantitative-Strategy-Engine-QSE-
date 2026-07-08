// QR3.4 — integrate the regime overlay with the A5 risk-aversion λ. Done-when:
// a state-change YAML injection forces the engine to scale down gross exposure
// / shift to a minimum-variance posture. The map is loaded from a YAML
// `regime_lambda` sequence; the crash regime selects a larger λ and the
// PortfolioBuilder optimum moves to lower variance (and lower gross on a
// factor-exposed book).

#include <gtest/gtest.h>

#include "qse/factor/PortfolioBuilder.h"
#include "qse/factor/RegimeLambda.h"

#include <cstdio>
#include <fstream>
#include <vector>

namespace {

const std::vector<std::string> kSymbols = {"A", "B", "C", "D"};

double variance_of(const std::vector<double>& weights, const std::vector<double>& sigmas) {
    double var = 0.0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        var += weights[i] * weights[i] * sigmas[i] * sigmas[i];
    }
    return var;
}

qse::PortfolioBuilder::OptimizationConfig base_config() {
    qse::PortfolioBuilder::OptimizationConfig config;
    config.gross_cap = 10.0;       // loose so the min-variance tilt, not the cap, drives the result
    config.max_iterations = 20000; // run to a sharp optimum
    config.convergence_tol = 1e-12;
    return config;
}

// A calm/elevated/turbulent map: calm is risk-neutral, turbulent is heavily
// risk-averse. Written to a temp YAML so we exercise the real loader.
std::string write_map_yaml() {
    const std::string path = "regime_lambda_test.yaml";
    std::ofstream ofs(path);
    ofs << "regime_lambda: [0.0, 5.0, 50.0]\n";
    return path;
}

} // namespace

TEST(RegimeLambdaTest, LoadsSequenceFromYamlAndMapsStates) {
    const std::string path = write_map_yaml();
    auto map = qse::RegimeLambda::from_file(path);
    std::remove(path.c_str());

    ASSERT_EQ(map.num_states(), 3u);
    EXPECT_DOUBLE_EQ(map.lambda_for(0), 0.0);   // calm
    EXPECT_DOUBLE_EQ(map.lambda_for(1), 5.0);   // elevated
    EXPECT_DOUBLE_EQ(map.lambda_for(2), 50.0);  // turbulent
    EXPECT_DOUBLE_EQ(map.lambda_for(-1), 0.0);  // clamp low
    EXPECT_DOUBLE_EQ(map.lambda_for(99), 50.0); // clamp high
}

TEST(RegimeLambdaTest, RejectsEmptyOrNegative) {
    EXPECT_THROW(qse::RegimeLambda({}), std::invalid_argument);
    EXPECT_THROW(qse::RegimeLambda({0.0, -1.0}), std::invalid_argument);
}

// Done-when (min-variance posture): switching the injected regime from calm to
// turbulent must lower the portfolio's variance.
TEST(RegimeLambdaTest, TurbulentRegimeShiftsToMinimumVariance) {
    const std::string path = write_map_yaml();
    auto map = qse::RegimeLambda::from_file(path);
    std::remove(path.c_str());

    const std::vector<double> alphas = {0.10, 0.10, -0.10, -0.10};
    const std::vector<double> betas = {0.0, 0.0, 0.0, 0.0}; // isolate the idiosyncratic term
    const std::vector<double> sigmas = {0.40, 0.10, 0.40, 0.10};

    qse::PortfolioBuilder calm;
    calm.set_config(map.apply(base_config(), /*state=*/0)); // λ = 0
    auto calm_w = calm.optimize(alphas, betas, sigmas, kSymbols).weights;

    qse::PortfolioBuilder turbulent;
    turbulent.set_config(map.apply(base_config(), /*state=*/2)); // λ = 50
    auto turb_w = turbulent.optimize(alphas, betas, sigmas, kSymbols).weights;

    EXPECT_LT(variance_of(turb_w, sigmas), variance_of(calm_w, sigmas))
        << "the turbulent regime's larger λ must shrink portfolio variance";
    // and it tilts toward the low-vol names (B, D) relative to the high-vol (A, C)
    EXPECT_GT(std::abs(turb_w[1] / turb_w[0]), std::abs(calm_w[1] / calm_w[0]));
}

// Done-when (scale down gross): on a factor-exposed book the market-variance
// channel makes the turbulent λ pull gross exposure in.
TEST(RegimeLambdaTest, TurbulentRegimeScalesDownGross) {
    auto map = qse::RegimeLambda({0.0, 5.0, 50.0});

    const std::vector<double> alphas = {0.10, 0.10, -0.10, -0.10};
    const std::vector<double> betas = {1.0, 1.0, -1.0, -1.0}; // βᵀw is large for the ±α book
    const std::vector<double> sigmas = {0.10, 0.10, 0.10, 0.10};

    auto run = [&](int state) {
        auto config = map.apply(base_config(), state);
        config.beta_tolerance = 1e9; // disable the beta projection: the market term does the work
        config.market_variance = 1.0;
        qse::PortfolioBuilder builder;
        builder.set_config(config);
        return builder.optimize(alphas, betas, sigmas, kSymbols).gross_exposure;
    };

    EXPECT_LT(run(/*turbulent=*/2), run(/*calm=*/0))
        << "the turbulent regime must scale the factor-exposed book's gross down";
}

TEST(RegimeLambdaTest, ApplyOnlyChangesLambda) {
    auto map = qse::RegimeLambda({1.0, 7.0});
    auto base = base_config();
    base.gamma = 0.033;
    base.gross_cap = 1.7;
    auto applied = map.apply(base, /*state=*/1);

    EXPECT_DOUBLE_EQ(applied.risk_aversion, 7.0); // set from the regime
    EXPECT_DOUBLE_EQ(applied.gamma, 0.033);       // everything else preserved
    EXPECT_DOUBLE_EQ(applied.gross_cap, 1.7);
    EXPECT_EQ(applied.max_iterations, base.max_iterations);
}
