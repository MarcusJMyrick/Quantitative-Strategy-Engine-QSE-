#pragma once

#include <arrow/table.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace qse {

// RiskModel computes ex-ante beta (and optional residual sigma) for each asset
class RiskModel {
public:
    struct Config {
        int    window        = 60;     // rolling look-back in days
        int    min_obs       = 60;     // min observations required to publish
        bool   apply_shrink  = false;  // shrink beta toward 1?
        double lambda        = 0.0;    // shrinkage intensity 0-1
    };

    RiskModel() = default;
    explicit RiskModel(const Config& cfg) : cfg_(cfg) {}

    void set_config(const Config& cfg) { cfg_ = cfg; }

    // Compute beta (+ resid sigma) and append to table
    // table must contain columns: date, asset, return, market_return
    std::shared_ptr<arrow::Table> append_beta(const std::shared_ptr<arrow::Table>& table,
                                              const std::string& asset_col,
                                              const std::string& date_col,
                                              const std::string& ret_col,
                                              const std::string& mkt_ret_col);

    // For standalone unit tests – compute beta series from vectors
    std::vector<double> rolling_beta(const std::vector<double>& asset_ret,
                                     const std::vector<double>& mkt_ret);

    std::vector<double> rolling_resid_sigma(const std::vector<double>& asset_ret,
                                            const std::vector<double>& mkt_ret,
                                            const std::vector<double>& beta_series);

    const Config& config() const { return cfg_; }

private:
    Config cfg_;
};

} // namespace qse 