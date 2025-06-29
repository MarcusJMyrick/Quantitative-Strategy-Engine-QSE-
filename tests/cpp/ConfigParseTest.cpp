#include <gtest/gtest.h>
#include "qse/strategy/FactorStrategyConfig.h"
#include <chrono>

using namespace qse;

class ConfigParseTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ConfigParseTest, RebalanceTime) {
    // Load factor_strategy.yaml that sets rebalance_time: "09:45"
    // Strategy instance reports rebalance_time == 9h45m
    
    std::string yaml_content = R"(
rebalance_time: "09:45"
min_dollar_threshold: 100.0
engine:
  order_style: "market"
  max_px_impact: 0.02
portfolio:
  initial_cash: 500000.0
)";
    
    FactorStrategyConfig config;
    ASSERT_TRUE(config.load_from_string(yaml_content));
    
    // Test rebalance time parsing
    auto rebalance_minutes = config.get_rebalance_time_minutes();
    auto expected_minutes = std::chrono::minutes(9 * 60 + 45); // 9h45m
    
    EXPECT_EQ(rebalance_minutes, expected_minutes);
    EXPECT_EQ(config.get_rebalance_time_string(), "09:45");
}

TEST_F(ConfigParseTest, Defaults) {
    // YAML omits optional fields
    // Defaults (min_dollar_threshold, etc.) take expected values
    
    std::string yaml_content = R"(
# Only specify rebalance time, omit other fields
rebalance_time: "14:30"
)";
    
    FactorStrategyConfig config;
    ASSERT_TRUE(config.load_from_string(yaml_content));
    
    // Test that defaults are applied
    EXPECT_EQ(config.get_rebalance_time_string(), "14:30");
    EXPECT_EQ(config.get_min_dollar_threshold(), 50.0); // Default value
    EXPECT_EQ(config.get_initial_cash(), 1000000.0); // Default value
    EXPECT_EQ(config.get_weights_directory(), "data/weights"); // Default value
    
    // Test engine config defaults
    const auto& engine_config = config.get_engine_config();
    EXPECT_EQ(engine_config.order_style, "market");
    EXPECT_EQ(engine_config.max_px_impact, 0.01);
    EXPECT_EQ(engine_config.min_notional, 100.0);
    EXPECT_EQ(engine_config.lot_size, 1);
    EXPECT_EQ(engine_config.min_qty, 1.0);
    
    // Test portfolio config defaults
    const auto& portfolio_config = config.get_portfolio_config();
    EXPECT_EQ(portfolio_config.initial_cash, 1000000.0);
    EXPECT_EQ(portfolio_config.max_position_size, 0.20);
    EXPECT_EQ(portfolio_config.max_leverage, 1.5);
    
    // Test data config defaults
    const auto& data_config = config.get_data_config();
    EXPECT_EQ(data_config.weights_directory, "data/weights");
    EXPECT_EQ(data_config.price_source, "close");
    
    // Test logging config defaults
    const auto& logging_config = config.get_logging_config();
    EXPECT_EQ(logging_config.level, "info");
    EXPECT_TRUE(logging_config.equity_curve);
    EXPECT_TRUE(logging_config.trade_log);
    EXPECT_TRUE(logging_config.performance);
}

TEST_F(ConfigParseTest, FullConfiguration) {
    // Test loading a complete configuration
    std::string yaml_content = R"(
rebalance_time: "16:00"
min_dollar_threshold: 75.0
engine:
  order_style: "target_percent"
  max_px_impact: 0.015
  min_notional: 200.0
  lot_size: 10
  min_qty: 5.0
portfolio:
  initial_cash: 2000000.0
  max_position_size: 0.15
  max_leverage: 2.0
data:
  weights_directory: "custom/weights"
  price_source: "vwap"
logging:
  level: "debug"
  equity_curve: false
  trade_log: true
  performance: false
)";
    
    FactorStrategyConfig config;
    ASSERT_TRUE(config.load_from_string(yaml_content));
    
    // Test all values are loaded correctly
    EXPECT_EQ(config.get_rebalance_time_string(), "16:00");
    EXPECT_EQ(config.get_min_dollar_threshold(), 75.0);
    EXPECT_EQ(config.get_initial_cash(), 2000000.0);
    EXPECT_EQ(config.get_weights_directory(), "custom/weights");
    
    // Test engine config
    const auto& engine_config = config.get_engine_config();
    EXPECT_EQ(engine_config.order_style, "target_percent");
    EXPECT_EQ(engine_config.max_px_impact, 0.015);
    EXPECT_EQ(engine_config.min_notional, 200.0);
    EXPECT_EQ(engine_config.lot_size, 10);
    EXPECT_EQ(engine_config.min_qty, 5.0);
    
    // Test portfolio config
    const auto& portfolio_config = config.get_portfolio_config();
    EXPECT_EQ(portfolio_config.initial_cash, 2000000.0);
    EXPECT_EQ(portfolio_config.max_position_size, 0.15);
    EXPECT_EQ(portfolio_config.max_leverage, 2.0);
    
    // Test data config
    const auto& data_config = config.get_data_config();
    EXPECT_EQ(data_config.weights_directory, "custom/weights");
    EXPECT_EQ(data_config.price_source, "vwap");
    
    // Test logging config
    const auto& logging_config = config.get_logging_config();
    EXPECT_EQ(logging_config.level, "debug");
    EXPECT_FALSE(logging_config.equity_curve);
    EXPECT_TRUE(logging_config.trade_log);
    EXPECT_FALSE(logging_config.performance);
}

TEST_F(ConfigParseTest, ExecConfigConversion) {
    // Test conversion to FactorExecutionEngine::ExecConfig
    std::string yaml_content = R"(
rebalance_time: "10:30"
engine:
  order_style: "market"
  max_px_impact: 0.025
  min_notional: 150.0
  lot_size: 5
  min_qty: 2.0
)";
    
    FactorStrategyConfig config;
    ASSERT_TRUE(config.load_from_string(yaml_content));
    
    ExecConfig exec_config = config.to_exec_config();
    
    EXPECT_EQ(exec_config.rebal_time, "10:30");
    EXPECT_EQ(exec_config.order_style, "market");
    EXPECT_EQ(exec_config.max_px_impact, 0.025);
    EXPECT_EQ(exec_config.min_notional, 150.0);
    EXPECT_EQ(exec_config.lot_size, 5);
    EXPECT_EQ(exec_config.min_qty, 2.0);
}

TEST_F(ConfigParseTest, InvalidTimeFormat) {
    // Test handling of invalid time format
    std::string yaml_content = R"(
rebalance_time: "25:70"  # Invalid time
min_dollar_threshold: 50.0
)";
    
    FactorStrategyConfig config;
    ASSERT_TRUE(config.load_from_string(yaml_content));
    
    // Should fall back to default time (15:45)
    auto rebalance_minutes = config.get_rebalance_time_minutes();
    auto default_minutes = std::chrono::minutes(15 * 60 + 45); // 15h45m
    
    EXPECT_EQ(rebalance_minutes, default_minutes);
} 