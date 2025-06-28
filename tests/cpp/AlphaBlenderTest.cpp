#include <gtest/gtest.h>
#include "qse/factor/AlphaBlender.h"
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/array/array_primitive.h>
#include <arrow/type.h>
#include <fstream>
#include <iostream>
#include <arrow/builder.h>
#include <arrow/status.h>

class AlphaBlenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data
        create_test_data();
        create_test_config();
    }
    
    void create_test_data() {
        // Create test table with factors and returns
        arrow::DoubleBuilder factor1_builder, factor2_builder, returns_builder;
        arrow::StringBuilder date_builder;
        
        // Test data: 3 dates, 3 symbols per date
        std::vector<std::string> dates = {"2023-01-01", "2023-01-01", "2023-01-01",
                                         "2023-01-02", "2023-01-02", "2023-01-02",
                                         "2023-01-03", "2023-01-03", "2023-01-03"};
        
        // Factor 1: ascending values
        std::vector<double> factor1_values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
        
        // Factor 2: descending values
        std::vector<double> factor2_values = {9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
        
        // Returns: mixed pattern
        std::vector<double> return_values = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
        
        auto status = factor1_builder.AppendValues(factor1_values);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = factor2_builder.AppendValues(factor2_values);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = returns_builder.AppendValues(return_values);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = date_builder.AppendValues(dates);
        ASSERT_TRUE(status.ok()) << status.ToString();
        
        std::shared_ptr<arrow::Array> factor1_array, factor2_array, returns_array, date_array;
        status = factor1_builder.Finish(&factor1_array);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = factor2_builder.Finish(&factor2_array);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = returns_builder.Finish(&returns_array);
        ASSERT_TRUE(status.ok()) << status.ToString();
        status = date_builder.Finish(&date_array);
        ASSERT_TRUE(status.ok()) << status.ToString();
        
        auto schema = arrow::schema({
            arrow::field("date", arrow::utf8()),
            arrow::field("factor1", arrow::float64()),
            arrow::field("factor2", arrow::float64()),
            arrow::field("returns", arrow::float64())
        });
        
        test_table_ = arrow::Table::Make(schema, {date_array, factor1_array, factor2_array, returns_array});
    }
    
    void create_test_config() {
        // Create test YAML config file
        std::ofstream config_file("test_alpha_blender_config.yaml");
        config_file << "factor_weights:\n";
        config_file << "  factor1: 0.6\n";
        config_file << "  factor2: 0.4\n";
        config_file << "use_ir_weighting: false\n";
        config_file << "ir_lookback_period: 252\n";
        config_file << "min_ir_weight: 0.1\n";
        config_file << "max_ir_weight: 2.0\n";
        config_file.close();
        
        // Create IR-weighted config
        std::ofstream ir_config_file("test_ir_blender_config.yaml");
        ir_config_file << "factor_weights:\n";
        ir_config_file << "  factor1: 0.5\n";
        ir_config_file << "  factor2: 0.5\n";
        ir_config_file << "use_ir_weighting: true\n";
        ir_config_file << "ir_lookback_period: 252\n";
        ir_config_file << "min_ir_weight: 0.1\n";
        ir_config_file << "max_ir_weight: 2.0\n";
        ir_config_file.close();
    }
    
    void TearDown() override {
        // Clean up test files
        std::remove("test_alpha_blender_config.yaml");
        std::remove("test_ir_blender_config.yaml");
    }
    
    std::shared_ptr<arrow::Table> test_table_;
};

TEST_F(AlphaBlenderTest, LoadConfig) {
    qse::AlphaBlender blender;
    
    EXPECT_TRUE(blender.load_config("test_alpha_blender_config.yaml"));
    
    auto config = blender.get_config();
    EXPECT_EQ(config.factor_weights["factor1"], 0.6);
    EXPECT_EQ(config.factor_weights["factor2"], 0.4);
    EXPECT_FALSE(config.use_ir_weighting);
    EXPECT_EQ(config.ir_lookback_period, 252);
    EXPECT_EQ(config.min_ir_weight, 0.1);
    EXPECT_EQ(config.max_ir_weight, 2.0);
}

TEST_F(AlphaBlenderTest, YAMLWeightedBlending) {
    qse::AlphaBlender blender;
    EXPECT_TRUE(blender.load_config("test_alpha_blender_config.yaml"));
    
    std::vector<std::string> factor_cols = {"factor1", "factor2"};
    auto result = blender.blend_factors(test_table_, factor_cols, "returns", "date");
    
    EXPECT_NE(result.table, nullptr);
    EXPECT_NE(result.table->schema()->GetFieldByName("alpha_score"), nullptr);
    
    // Check that alpha_score column exists and has correct values
    auto alpha_array = std::static_pointer_cast<arrow::DoubleArray>(
        result.table->GetColumnByName("alpha_score")->chunk(0));
    
    EXPECT_EQ(alpha_array->length(), 9);
    
    // Verify first row: 0.6 * 1.0 + 0.4 * 9.0 = 0.6 + 3.6 = 4.2
    EXPECT_NEAR(alpha_array->Value(0), 4.2, 1e-6);
    
    // Verify weights
    EXPECT_EQ(result.final_weights["factor1"], 0.6);
    EXPECT_EQ(result.final_weights["factor2"], 0.4);
}

TEST_F(AlphaBlenderTest, IRWeightedBlending) {
    qse::AlphaBlender blender;
    EXPECT_TRUE(blender.load_config("test_ir_blender_config.yaml"));
    
    std::vector<std::string> factor_cols = {"factor1", "factor2"};
    auto result = blender.blend_factors(test_table_, factor_cols, "returns", "date");
    
    EXPECT_NE(result.table, nullptr);
    EXPECT_NE(result.table->schema()->GetFieldByName("alpha_score"), nullptr);
    
    // Check that IR values were calculated
    EXPECT_FALSE(result.factor_irs.empty());
    EXPECT_TRUE(result.factor_irs.find("factor1") != result.factor_irs.end());
    EXPECT_TRUE(result.factor_irs.find("factor2") != result.factor_irs.end());
    
    // Check that weights were calculated based on IR
    EXPECT_FALSE(result.final_weights.empty());
    EXPECT_TRUE(result.final_weights.find("factor1") != result.final_weights.end());
    EXPECT_TRUE(result.final_weights.find("factor2") != result.final_weights.end());
    
    // Weights should sum to 1.0
    double total_weight = 0.0;
    for (const auto& weight : result.final_weights) {
        total_weight += weight.second;
    }
    EXPECT_NEAR(total_weight, 1.0, 1e-6);
}

TEST_F(AlphaBlenderTest, CalculateIR) {
    qse::AlphaBlender blender;
    
    // Test perfect positive correlation
    std::vector<double> factor_values = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> return_values = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    double ir = blender.calculate_ir(factor_values, return_values);
    EXPECT_NEAR(ir, 1.0, 1e-6);
    
    // Test perfect negative correlation
    std::vector<double> neg_return_values = {5.0, 4.0, 3.0, 2.0, 1.0};
    ir = blender.calculate_ir(factor_values, neg_return_values);
    EXPECT_NEAR(ir, -1.0, 1e-6);
    
    // Test no correlation
    std::vector<double> no_corr_values = {1.0, 1.0, 1.0, 1.0, 1.0};
    ir = blender.calculate_ir(factor_values, no_corr_values);
    EXPECT_NEAR(ir, 0.0, 1e-6);
}

TEST_F(AlphaBlenderTest, SetConfigDirectly) {
    qse::AlphaBlender blender;
    
    qse::AlphaBlender::BlendingConfig config;
    config.factor_weights["factor1"] = 0.7;
    config.factor_weights["factor2"] = 0.3;
    config.use_ir_weighting = false;
    
    blender.set_config(config);
    
    auto retrieved_config = blender.get_config();
    EXPECT_EQ(retrieved_config.factor_weights["factor1"], 0.7);
    EXPECT_EQ(retrieved_config.factor_weights["factor2"], 0.3);
    EXPECT_FALSE(retrieved_config.use_ir_weighting);
}

TEST_F(AlphaBlenderTest, WeightNormalization) {
    qse::AlphaBlender blender;
    
    // Test with weights that don't sum to 1.0
    qse::AlphaBlender::BlendingConfig config;
    config.factor_weights["factor1"] = 0.6;
    config.factor_weights["factor2"] = 0.6; // Total = 1.2
    config.use_ir_weighting = false;
    
    blender.set_config(config);
    
    std::vector<std::string> factor_cols = {"factor1", "factor2"};
    auto result = blender.blend_factors(test_table_, factor_cols, "returns", "date");
    
    // Weights should be normalized to sum to 1.0
    EXPECT_NEAR(result.final_weights["factor1"], 0.5, 1e-6);
    EXPECT_NEAR(result.final_weights["factor2"], 0.5, 1e-6);
}

TEST_F(AlphaBlenderTest, MissingFactorWeights) {
    qse::AlphaBlender blender;
    
    qse::AlphaBlender::BlendingConfig config;
    config.factor_weights["factor1"] = 1.0;
    // factor2 not specified
    config.use_ir_weighting = false;
    
    blender.set_config(config);
    
    std::vector<std::string> factor_cols = {"factor1", "factor2"};
    auto result = blender.blend_factors(test_table_, factor_cols, "returns", "date");
    
    // Only factor1 should be used
    EXPECT_EQ(result.final_weights["factor1"], 1.0);
    EXPECT_EQ(result.final_weights.find("factor2"), result.final_weights.end());
    
    // Alpha score should only include factor1
    auto alpha_array = std::static_pointer_cast<arrow::DoubleArray>(
        result.table->GetColumnByName("alpha_score")->chunk(0));
    EXPECT_NEAR(alpha_array->Value(0), 1.0, 1e-6); // factor1 value at index 0
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 