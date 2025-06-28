#include <gtest/gtest.h>
#include "qse/factor/UniverseFilter.h"
#include <arrow/array.h>
#include <arrow/table.h>
#include <arrow/builder.h>
#include <random>

using namespace qse;

static std::shared_ptr<arrow::Table> random_dirty_table(int rows) {
    std::mt19937 rng(777);
    std::uniform_real_distribution<double> price_d(0.0, 100.0);
    std::uniform_real_distribution<double> vol_d(0.0, 2e6);
    std::bernoulli_distribution nan_coin(0.05);
    
    arrow::DoubleBuilder price_b, vol_b;
    for (int i=0;i<rows;++i) {
        double p = price_d(rng);
        double v = vol_d(rng);
        if (nan_coin(rng)) p = std::numeric_limits<double>::quiet_NaN();
        if (nan_coin(rng)) v = std::numeric_limits<double>::infinity();
        price_b.Append(p);
        vol_b.Append(v);
    }
    std::shared_ptr<arrow::Array> price_a, vol_a; price_b.Finish(&price_a); vol_b.Finish(&vol_a);
    auto schema = arrow::schema({arrow::field("close",arrow::float64()),arrow::field("volume",arrow::float64())});
    return arrow::Table::Make(schema,{price_a,vol_a});
}

TEST(UniverseFuzzTest, CleanDataRemovesNaNInf) {
    auto dirty = random_dirty_table(500);
    UniverseFilter uf;
    auto cleaned = uf.clean_data(dirty);
    ASSERT_TRUE(cleaned!=nullptr);
    EXPECT_TRUE(uf.validate_no_nan(cleaned));
} 