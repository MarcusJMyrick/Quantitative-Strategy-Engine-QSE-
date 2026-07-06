// Efficient-frontier sweep (roadmap A5): runs the mean-variance
// PortfolioBuilder over a log-spaced grid of risk-aversion values λ against
// a fixed synthetic universe and records (λ, expected alpha, variance,
// gross) per point. scripts/analysis/efficient_frontier.py plots the
// resulting frontier.
//
// Output CSV: lambda,exp_alpha,variance,stdev,gross

#include "qse/factor/PortfolioBuilder.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string out_path = "results/frontier_sweep.csv";
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        if (flag == "--out") {
            out_path = argv[i + 1];
        } else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    // Fixed synthetic universe: dispersion in alpha, beta, and idio vol so
    // the frontier has real trade-offs to reveal
    const std::vector<std::string> symbols = {"A", "B", "C", "D", "E", "F", "G", "H"};
    const std::vector<double> alphas = {0.12, 0.08, 0.05, 0.02, -0.02, -0.05, -0.08, -0.12};
    const std::vector<double> betas = {1.4, 0.7, 1.1, 0.9, 1.0, 0.8, 1.2, 0.6};
    const std::vector<double> sigmas = {0.35, 0.12, 0.28, 0.18, 0.22, 0.15, 0.30, 0.10};
    const double market_variance = 0.04; // σ_m = 20%

    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Could not open " << out_path << "\n";
        return 1;
    }
    out << "lambda,exp_alpha,variance,stdev,gross\n";

    auto portfolio_variance = [&](const std::vector<double>& w) {
        double beta_dot_w = 0.0;
        double idio = 0.0;
        for (std::size_t i = 0; i < w.size(); ++i) {
            beta_dot_w += betas[i] * w[i];
            idio += w[i] * w[i] * sigmas[i] * sigmas[i];
        }
        return market_variance * beta_dot_w * beta_dot_w + idio;
    };

    // 20 log-spaced risk-aversion points: 0.25 * 1.5^i, up to ~554
    for (int i = 0; i < 20; ++i) {
        const double lambda = 0.25 * std::pow(1.5, i);
        qse::PortfolioBuilder builder;
        qse::PortfolioBuilder::OptimizationConfig config;
        config.risk_aversion = lambda;
        config.market_variance = market_variance;
        config.gross_cap = 4.0;
        config.max_iterations = 20000;
        config.convergence_tol = 1e-12;
        builder.set_config(config);

        auto result = builder.optimize(alphas, betas, sigmas, symbols);

        double exp_alpha = 0.0;
        for (std::size_t i = 0; i < result.weights.size(); ++i) {
            exp_alpha += alphas[i] * result.weights[i];
        }
        const double variance = portfolio_variance(result.weights);

        out << lambda << ',' << exp_alpha << ',' << variance << ',' << std::sqrt(variance) << ','
            << result.gross_exposure << '\n';
    }

    std::cout << "Frontier sweep written to " << out_path << "\n";
    return 0;
}
