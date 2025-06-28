#include "qse/factor/RiskModel.h"
#include "qse/math/StatsUtil.h"
#include <arrow/array.h>
#include <arrow/builder.h>
#include <limits>

using namespace qse;
using namespace qse::math;

std::vector<double> RiskModel::rolling_beta(const std::vector<double>& asset_ret,
                                            const std::vector<double>& mkt_ret) {
    size_t n = asset_ret.size();
    std::vector<double> beta(n, std::numeric_limits<double>::quiet_NaN());
    RollingCovariance cov(cfg_.window);
    RollingVariance   var(cfg_.window);

    for (size_t i = 0; i < n; ++i) {
        double c = cov(asset_ret[i], mkt_ret[i]);
        double v = var(mkt_ret[i]);
        if (cov.count() >= static_cast<size_t>(cfg_.min_obs) && v != 0.0) {
            double b = c / v;
            if (cfg_.apply_shrink) {
                // shrink towards 1.0
                b = cfg_.lambda * 1.0 + (1.0 - cfg_.lambda) * b;
            }
            beta[i] = b;
        }
    }
    return beta;
}

std::vector<double> RiskModel::rolling_resid_sigma(const std::vector<double>& asset_ret,
                                                   const std::vector<double>& mkt_ret,
                                                   const std::vector<double>& beta_series) {
    size_t n = asset_ret.size();
    std::vector<double> sigma(n, std::numeric_limits<double>::quiet_NaN());
    RollingStdDev sd(cfg_.window);
    for (size_t i = 0; i < n; ++i) {
        if (!std::isnan(beta_series[i])) {
            double resid = asset_ret[i] - beta_series[i] * mkt_ret[i];
            double s = sd(resid);
            if (sd.count() >= static_cast<size_t>(cfg_.min_obs)) {
                sigma[i] = s;
            }
        } else {
            // feed dummy to keep window consistent
            sd(0.0);
        }
    }
    return sigma;
}

std::shared_ptr<arrow::Table> RiskModel::append_beta(const std::shared_ptr<arrow::Table>& table,
                                                     const std::string& asset_col,
                                                     const std::string& date_col,
                                                     const std::string& ret_col,
                                                     const std::string& mkt_ret_col) {
    // NOTE: Simplified implementation – assumes table is already scoped to one asset and sorted by date
    // In practice, you'd group_by asset then call rolling_beta per asset.

    auto ret_array     = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName(ret_col)->chunk(0));
    auto mkt_ret_array = std::static_pointer_cast<arrow::DoubleArray>(table->GetColumnByName(mkt_ret_col)->chunk(0));
    size_t n = ret_array->length();

    std::vector<double> asset_ret(n), mkt_ret(n);
    for (size_t i = 0; i < n; ++i) {
        asset_ret[i] = ret_array->Value(i);
        mkt_ret[i]   = mkt_ret_array->Value(i);
    }

    auto beta = rolling_beta(asset_ret, mkt_ret);
    auto resid_sigma = rolling_resid_sigma(asset_ret, mkt_ret, beta);

    // Build Arrow arrays
    arrow::DoubleBuilder beta_bld, sigma_bld;
    beta_bld.AppendValues(beta);
    sigma_bld.AppendValues(resid_sigma);
    std::shared_ptr<arrow::Array> beta_arr, sigma_arr;
    beta_bld.Finish(&beta_arr);
    sigma_bld.Finish(&sigma_arr);

    auto beta_field  = arrow::field("beta", arrow::float64());
    auto sigma_field = arrow::field("resid_sigma", arrow::float64());

    std::vector<std::shared_ptr<arrow::Field>> fields = table->schema()->fields();
    fields.push_back(beta_field);
    fields.push_back(sigma_field);

    std::vector<std::shared_ptr<arrow::ChunkedArray>> chunks = table->columns();
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    for (auto& ca : chunks) arrays.push_back(ca->chunk(0));
    arrays.push_back(beta_arr);
    arrays.push_back(sigma_arr);

    auto schema = arrow::schema(fields);
    return arrow::Table::Make(schema, arrays);
} 