#include <gtest/gtest.h>
#include "qse/factor/AlphaBlender.h"
#include "qse/factor/ICMonitor.h"
#include <arrow/array.h>
#include <arrow/table.h>
#include <arrow/builder.h>

using namespace qse;

// Helper to build a simple Arrow table with two factors + returns
static std::shared_ptr<arrow::Table> build_table() {
    const int n = 100;
    std::vector<std::string> dates(n);
    std::vector<double> factor_strong(n); // high IC
    std::vector<double> factor_weak(n);   // low IC
    std::vector<double> returns(n);
    for (int i = 0; i < n; ++i) {
        dates[i] = "2023-01-" + std::to_string(1 + i);
        factor_strong[i] = static_cast<double>(i);      // perfectly correlated with returns
        factor_weak[i]   = (i % 2 == 0) ? 1.0 : -1.0;   // noisy signal
        returns[i]       = static_cast<double>(i);
    }
    // Build Arrow arrays
    arrow::StringBuilder sb; sb.AppendValues(dates);
    arrow::DoubleBuilder f1b, f2b, rb;
    f1b.AppendValues(factor_strong);
    f2b.AppendValues(factor_weak);
    rb.AppendValues(returns);
    std::shared_ptr<arrow::Array> date_a, f1_a, f2_a, r_a;
    sb.Finish(&date_a); f1b.Finish(&f1_a); f2b.Finish(&f2_a); rb.Finish(&r_a);
    auto schema = arrow::schema({
        arrow::field("date", arrow::utf8()),
        arrow::field("factorA", arrow::float64()),
        arrow::field("factorB", arrow::float64()),
        arrow::field("ret_fwd", arrow::float64())});
    return arrow::Table::Make(schema,{date_a,f1_a,f2_a,r_a});
}

TEST(AlphaBlenderICIntegrationTest, IRWeightsPrioritiseHighIC) {
    auto table = build_table();
    std::vector<std::string> factors = {"factorA","factorB"};

    // Config: use IR weighting
    qse::AlphaBlender blender;
    qse::AlphaBlender::BlendingConfig cfg;
    cfg.use_ir_weighting = true;
    cfg.min_ir_weight = 0.1;
    cfg.max_ir_weight = 2.0;
    blender.set_config(cfg);

    auto res = blender.blend_factors(table,factors,"ret_fwd","date");

    // Expect weight for factorA > factorB due to higher IC
    double wA = res.final_weights["factorA"];
    double wB = res.final_weights["factorB"];
    EXPECT_GT(wA, wB);
    EXPECT_NEAR(wA + wB, 1.0, 1e-6);
} 