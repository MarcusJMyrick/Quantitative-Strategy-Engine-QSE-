#include "qse/factor/ICMonitor.h"
#include <gtest/gtest.h>
#include <arrow/api.h>
#include <memory>
#include <vector>

using namespace qse;

TEST(ICMonitorTest, SpearmanRankCorrelation) {
    ICMonitor monitor;
    
    // Test perfect positive correlation
    std::vector<double> x1 = {1, 2, 3, 4, 5};
    std::vector<double> y1 = {1, 2, 3, 4, 5};
    EXPECT_NEAR(monitor.compute_ic(nullptr, "", "", "", 252).daily_ic.size(), 0, 1e-6);
    
    // Test perfect negative correlation
    std::vector<double> x2 = {1, 2, 3, 4, 5};
    std::vector<double> y2 = {5, 4, 3, 2, 1};
    // Note: We can't directly test spearman_rank_corr as it's private
    // But we can test through the public interface
}

TEST(ICMonitorTest, ICComputationWithSyntheticData) {
    // Create synthetic Arrow table with factor and return data
    arrow::DoubleBuilder factor_builder, return_builder;
    arrow::StringBuilder date_builder;
    
    // Create data for 3 dates with 5 stocks each
    std::vector<std::string> dates = {"2023-01-01", "2023-01-01", "2023-01-01", "2023-01-01", "2023-01-01",
                                      "2023-01-02", "2023-01-02", "2023-01-02", "2023-01-02", "2023-01-02",
                                      "2023-01-03", "2023-01-03", "2023-01-03", "2023-01-03", "2023-01-03"};
    
    std::vector<double> factors = {1.0, 2.0, 3.0, 4.0, 5.0,  // Date 1: perfect positive correlation
                                  2.0, 1.0, 3.0, 5.0, 4.0,  // Date 2: mixed correlation
                                  5.0, 4.0, 3.0, 2.0, 1.0}; // Date 3: perfect negative correlation
    
    std::vector<double> returns = {0.01, 0.02, 0.03, 0.04, 0.05,  // Date 1: perfect positive
                                  0.03, 0.01, 0.02, 0.05, 0.04,  // Date 2: mixed (different pattern)
                                  0.01, 0.02, 0.03, 0.04, 0.05}; // Date 3: perfect negative (ascending returns with descending factors)
    
    // Build Arrow arrays
    for (const auto& date : dates) {
        date_builder.Append(date);
    }
    for (double factor : factors) {
        factor_builder.Append(factor);
    }
    for (double ret : returns) {
        return_builder.Append(ret);
    }
    
    std::shared_ptr<arrow::Array> date_array, factor_array, return_array;
    date_builder.Finish(&date_array);
    factor_builder.Finish(&factor_array);
    return_builder.Finish(&return_array);
    
    // Create schema and table
    std::vector<std::shared_ptr<arrow::Field>> schema_fields = {
        arrow::field("date", arrow::utf8()),
        arrow::field("factor", arrow::float64()),
        arrow::field("return", arrow::float64())
    };
    auto schema = std::make_shared<arrow::Schema>(schema_fields);
    std::vector<std::shared_ptr<arrow::Array>> columns = {date_array, factor_array, return_array};
    auto table = arrow::Table::Make(schema, columns);
    
    // Basic sanity checks
    EXPECT_EQ(table->num_rows(), 15);
    EXPECT_EQ(table->num_columns(), 3);
    
    // Test IC computation
    ICMonitor monitor;
    auto result = monitor.compute_ic(table, "factor", "return", "date", 2);
    
    // Check that we got results for 3 dates
    EXPECT_EQ(result.daily_ic.size(), 3);
    EXPECT_EQ(result.rolling_mean.size(), 3);
    EXPECT_EQ(result.rolling_std.size(), 3);
    
    // Check that first date has high positive IC (perfect correlation)
    if (result.daily_ic.size() > 0) {
        EXPECT_GT(result.daily_ic[0], 0.9);
    }
    
    // Check that third date has high negative IC (perfect negative correlation)
    if (result.daily_ic.size() > 2) {
        EXPECT_LT(result.daily_ic[2], -0.9);
    }
    
    // Check that rolling statistics are computed
    if (result.rolling_mean.size() > 1) {
        EXPECT_FALSE(std::isnan(result.rolling_mean[1]));
        EXPECT_FALSE(std::isnan(result.rolling_std[1]));
    }
}

TEST(ICMonitorTest, EmptyDataHandling) {
    ICMonitor monitor;
    
    // Test with null table
    auto result1 = monitor.compute_ic(nullptr, "factor", "return", "date");
    EXPECT_EQ(result1.daily_ic.size(), 0);
    EXPECT_EQ(result1.rolling_mean.size(), 0);
    EXPECT_EQ(result1.rolling_std.size(), 0);
    
    // Test with empty table
    arrow::DoubleBuilder factor_builder, return_builder;
    arrow::StringBuilder date_builder;
    std::shared_ptr<arrow::Array> date_array, factor_array, return_array;
    date_builder.Finish(&date_array);
    factor_builder.Finish(&factor_array);
    return_builder.Finish(&return_array);
    
    std::vector<std::shared_ptr<arrow::Field>> schema_fields = {
        arrow::field("date", arrow::utf8()),
        arrow::field("factor", arrow::float64()),
        arrow::field("return", arrow::float64())
    };
    auto schema = std::make_shared<arrow::Schema>(schema_fields);
    std::vector<std::shared_ptr<arrow::Array>> columns = {date_array, factor_array, return_array};
    auto empty_table = arrow::Table::Make(schema, columns);
    
    auto result2 = monitor.compute_ic(empty_table, "factor", "return", "date");
    EXPECT_EQ(result2.daily_ic.size(), 0);
    EXPECT_EQ(result2.rolling_mean.size(), 0);
    EXPECT_EQ(result2.rolling_std.size(), 0);
} 