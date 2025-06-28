#include "qse/factor/CrossSectionalRegression.h"
#include <gtest/gtest.h>
#include <arrow/api.h>
#include <memory>
#include <vector>

using namespace qse;

TEST(CrossSectionalRegressionTest, OLSRegressionSimple) {
    // Create synthetic Arrow table with 2 factors and returns
    arrow::DoubleBuilder factor1_builder, factor2_builder, return_builder;
    std::vector<double> factor1 = {1, 2, 3, 4, 5};
    std::vector<double> factor2 = {2, 1, 0, 1, 2};
    std::vector<double> returns = {2, 3, 2, 5, 7};
    for (double v : factor1) factor1_builder.Append(v);
    for (double v : factor2) factor2_builder.Append(v);
    for (double v : returns) return_builder.Append(v);
    std::shared_ptr<arrow::Array> factor1_array, factor2_array, return_array;
    factor1_builder.Finish(&factor1_array);
    factor2_builder.Finish(&factor2_array);
    return_builder.Finish(&return_array);

    std::vector<std::shared_ptr<arrow::Field>> schema_fields = {
        arrow::field("factor1", arrow::float64()),
        arrow::field("factor2", arrow::float64()),
        arrow::field("returns", arrow::float64())
    };
    auto schema = std::make_shared<arrow::Schema>(schema_fields);
    std::vector<std::shared_ptr<arrow::Array>> columns = {factor1_array, factor2_array, return_array};
    auto table = arrow::Table::Make(schema, columns);

    CrossSectionalRegression reg;
    std::vector<std::string> factor_cols = {"factor1", "factor2"};
    auto result = reg.run_regression(table, "", "returns", factor_cols);

    // Check that regression ran and returns are reasonable
    EXPECT_EQ(result.num_factors, 2);
    EXPECT_EQ(result.num_observations, 5);
    EXPECT_EQ(result.factor_returns.size(), 2);
    EXPECT_NEAR(result.total_r_squared, 0.9, 0.2); // Should fit decently
} 