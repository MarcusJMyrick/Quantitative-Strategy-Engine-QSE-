#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "qse/factor/PortfolioBuilder.h"

namespace qse {

/**
 * @class RegimeLambda
 * @brief Maps a (debounced) regime state to the A5 risk-aversion λ.
 *
 * QR3.4 — the risk overlay's integration point. The HMM (QR3.2) + anti-whipsaw
 * (QR3.3) produce a committed regime state (0 = calm … K−1 = turbulent, sorted
 * by realized vol). This maps that state to a λ for `PortfolioBuilder`: a
 * turbulent regime selects a larger λ, so the mean-variance term −λ/2·wᵀΣw
 * dominates and the optimizer shifts toward a minimum-variance / lower-gross
 * posture. The map is configuration, loaded from a YAML `regime_lambda`
 * sequence indexed by state.
 *
 * Convention (not hard-enforced, but the intent): λ should be non-decreasing in
 * state, so a more turbulent regime is at least as risk-averse as a calmer one.
 */
class RegimeLambda {
public:
    explicit RegimeLambda(std::vector<double> lambdas) : lambdas_(std::move(lambdas)) {
        if (lambdas_.empty()) {
            throw std::invalid_argument("RegimeLambda: regime_lambda must be non-empty");
        }
        for (double v : lambdas_) {
            if (v < 0.0) {
                throw std::invalid_argument("RegimeLambda: λ must be non-negative");
            }
        }
    }

    /// Build from a YAML sequence node, e.g. the value of `regime_lambda`.
    static RegimeLambda from_node(const YAML::Node& seq) {
        if (!seq || !seq.IsSequence()) {
            throw std::invalid_argument("RegimeLambda: expected a YAML sequence");
        }
        std::vector<double> lambdas;
        for (const auto& v : seq) {
            lambdas.push_back(v.as<double>());
        }
        return RegimeLambda(std::move(lambdas));
    }

    /// Load the `key` sequence (default "regime_lambda") from a YAML file.
    static RegimeLambda from_file(const std::string& yaml_path,
                                  const std::string& key = "regime_lambda") {
        YAML::Node root = YAML::LoadFile(yaml_path);
        if (!root[key]) {
            throw std::invalid_argument("RegimeLambda: key '" + key + "' not found in " +
                                        yaml_path);
        }
        return from_node(root[key]);
    }

    std::size_t num_states() const { return lambdas_.size(); }

    /// λ for a state, clamped to [0, K−1] (an out-of-range state uses the
    /// nearest configured regime rather than throwing mid-backtest).
    double lambda_for(int state) const {
        if (state < 0) {
            state = 0;
        }
        if (state >= static_cast<int>(lambdas_.size())) {
            state = static_cast<int>(lambdas_.size()) - 1;
        }
        return lambdas_[static_cast<std::size_t>(state)];
    }

    /// Copy of `base` with only risk_aversion overwritten by this regime's λ.
    PortfolioBuilder::OptimizationConfig apply(PortfolioBuilder::OptimizationConfig base,
                                               int state) const {
        base.risk_aversion = lambda_for(state);
        return base;
    }

private:
    std::vector<double> lambdas_;
};

} // namespace qse
