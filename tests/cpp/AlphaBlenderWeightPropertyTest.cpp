#include <gtest/gtest.h>
#include "qse/factor/AlphaBlender.h"
#include <random>
#include <arrow/builder.h>

using namespace qse;

TEST(AlphaBlenderWeightPropertyTest, NormalizationSumsToOne) {
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> ud(0.1, 10.0);

    for (int iter=0; iter<100; ++iter) {
        // Generate 5 random weights
        qse::AlphaBlender::BlendingConfig cfg;
        cfg.use_ir_weighting = false;
        cfg.factor_weights.clear();
        for (int i=0;i<5;++i) {
            cfg.factor_weights["f"+std::to_string(i)] = ud(rng);
        }
        AlphaBlender ab; ab.set_config(cfg);

        // Dummy table with 1 row per factor (values = 1.0)
        arrow::DoubleBuilder db; db.Append(1.0); std::shared_ptr<arrow::Array> arr; db.Finish(&arr);
        auto schema = arrow::schema({arrow::field("dummy", arrow::float64())});
        auto table = arrow::Table::Make(schema,{arr});

        auto res = ab.blend_factors(table, std::vector<std::string>{}, "dummy", "");
        double sum = 0.0; for (auto& kv : res.final_weights) sum += kv.second;
        EXPECT_NEAR(sum, 1.0, 1e-6);
    }
} 