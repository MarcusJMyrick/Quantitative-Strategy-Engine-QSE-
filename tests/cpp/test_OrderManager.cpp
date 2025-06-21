#include <gtest/gtest.h>
#include "qse/order/OrderManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace qse;

class OrderManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary file paths for testing
        equity_path_ = "test_equity.csv";
        tradelog_path_ = "test_tradelog.csv";
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove(equity_path_);
        std::filesystem::remove(tradelog_path_);
    }

    std::string equity_path_;
    std::string tradelog_path_;
};

TEST_F(OrderManagerTest, InitialState) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    EXPECT_EQ(manager.get_cash(), 100000.0);
    EXPECT_EQ(manager.get_position("AAPL"), 0);
    EXPECT_EQ(manager.get_position("GOOG"), 0);
}

TEST_F(OrderManagerTest, ExecuteBuyWithCosts) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // Buy 100 shares of AAPL at $150
    manager.execute_buy("AAPL", 100, 150.0);
    
    EXPECT_EQ(manager.get_cash(), 100000.0 - (100 * 150.0));
    EXPECT_EQ(manager.get_position("AAPL"), 100);
    EXPECT_EQ(manager.get_position("GOOG"), 0); // Should still be 0
}

TEST_F(OrderManagerTest, ExecuteSellWithCosts) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // First buy some shares
    manager.execute_buy("AAPL", 100, 150.0);
    
    // Then sell 50 shares at $160
    manager.execute_sell("AAPL", 50, 160.0);
    
    double expected_cash = 100000.0 - (100 * 150.0) + (50 * 160.0);
    EXPECT_EQ(manager.get_cash(), expected_cash);
    EXPECT_EQ(manager.get_position("AAPL"), 50);
}

TEST_F(OrderManagerTest, PortfolioValueIsCorrectAfterTrade) {
    {
        OrderManager manager(100000.0, equity_path_, tradelog_path_);
        
        // Buy 100 shares of AAPL at $150
        manager.execute_buy("AAPL", 100, 150.0);
        
        // Record equity with current market prices
        std::map<std::string, double> market_prices = {{"AAPL", 160.0}};
        manager.record_equity(1234567890, market_prices);
        
        // Check that files were created and contain data
        EXPECT_TRUE(std::filesystem::exists(equity_path_));
        EXPECT_TRUE(std::filesystem::exists(tradelog_path_));
    } // Manager goes out of scope here, files are closed and flushed
    
    // Read equity file to verify content
    std::ifstream equity_file(equity_path_);
    std::string line;
    std::getline(equity_file, line); // Skip header
    std::getline(equity_file, line); // Read data line
    
    EXPECT_FALSE(line.empty());
    EXPECT_NE(line.find("1234567890"), std::string::npos);
}

TEST_F(OrderManagerTest, ShortSelling) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // Short sell 50 shares of AAPL at $150
    manager.execute_sell("AAPL", 50, 150.0);
    
    EXPECT_EQ(manager.get_cash(), 100000.0 + (50 * 150.0));
    EXPECT_EQ(manager.get_position("AAPL"), -50);
}

TEST_F(OrderManagerTest, MultipleSymbols) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // Trade multiple symbols - adjust quantities to fit within cash constraints
    manager.execute_buy("AAPL", 100, 150.0);  // Cost: 15,000
    manager.execute_buy("GOOG", 5, 2500.0);   // Cost: 12,500 (total: 27,500, cash: 72,500)
    manager.execute_sell("MSFT", 25, 300.0);  // Revenue: 7,500 (cash: 80,000)
    
    EXPECT_EQ(manager.get_position("AAPL"), 100);
    EXPECT_EQ(manager.get_position("GOOG"), 5);
    EXPECT_EQ(manager.get_position("MSFT"), -25);
    EXPECT_EQ(manager.get_position("UNKNOWN"), 0); // Should return 0 for unknown symbol
}

TEST_F(OrderManagerTest, ComplexTradingScenario) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // Initial state
    EXPECT_EQ(manager.get_cash(), 100000.0);
    
    // Buy AAPL
    manager.execute_buy("AAPL", 200, 150.0);
    EXPECT_EQ(manager.get_cash(), 100000.0 - (200 * 150.0));
    EXPECT_EQ(manager.get_position("AAPL"), 200);
    
    // Buy GOOG
    manager.execute_buy("GOOG", 10, 2500.0);
    EXPECT_EQ(manager.get_cash(), 100000.0 - (200 * 150.0) - (10 * 2500.0));
    EXPECT_EQ(manager.get_position("GOOG"), 10);
    
    // Sell some AAPL
    manager.execute_sell("AAPL", 50, 160.0);
    double expected_cash = 100000.0 - (200 * 150.0) - (10 * 2500.0) + (50 * 160.0);
    EXPECT_EQ(manager.get_cash(), expected_cash);
    EXPECT_EQ(manager.get_position("AAPL"), 150);
    
    // Short MSFT
    manager.execute_sell("MSFT", 100, 300.0);
    expected_cash += (100 * 300.0);
    EXPECT_EQ(manager.get_cash(), expected_cash);
    EXPECT_EQ(manager.get_position("MSFT"), -100);
}

TEST_F(OrderManagerTest, EquityRecordingWithMultipleSymbols) {
    {
        OrderManager manager(100000.0, equity_path_, tradelog_path_);
        
        // Create positions
        manager.execute_buy("AAPL", 100, 150.0);
        manager.execute_buy("GOOG", 20, 2500.0);
        manager.execute_sell("MSFT", 50, 300.0);
        
        // Record equity with current market prices
        std::map<std::string, double> market_prices = {
            {"AAPL", 160.0},
            {"GOOG", 2600.0},
            {"MSFT", 280.0}
        };
        manager.record_equity(1234567890, market_prices);
        
        // Check that files were created
        EXPECT_TRUE(std::filesystem::exists(equity_path_));
        EXPECT_TRUE(std::filesystem::exists(tradelog_path_));
    } // Manager goes out of scope here, files are closed and flushed
    
    // Verify equity file content
    std::ifstream equity_file(equity_path_);
    std::string line;
    std::getline(equity_file, line); // Skip header
    std::getline(equity_file, line); // Read data line
    
    EXPECT_FALSE(line.empty());
    EXPECT_NE(line.find("1234567890"), std::string::npos);
}

TEST_F(OrderManagerTest, ZeroQuantityTrades) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // These should not change anything
    manager.execute_buy("AAPL", 0, 150.0);
    manager.execute_sell("GOOG", 0, 2500.0);
    
    EXPECT_EQ(manager.get_cash(), 100000.0);
    EXPECT_EQ(manager.get_position("AAPL"), 0);
    EXPECT_EQ(manager.get_position("GOOG"), 0);
}

TEST_F(OrderManagerTest, NegativeQuantityHandling) {
    OrderManager manager(100000.0, equity_path_, tradelog_path_);
    
    // These should be ignored
    manager.execute_buy("AAPL", -10, 150.0);
    manager.execute_sell("GOOG", -5, 2500.0);
    
    EXPECT_EQ(manager.get_cash(), 100000.0);
    EXPECT_EQ(manager.get_position("AAPL"), 0);
    EXPECT_EQ(manager.get_position("GOOG"), 0);
}

TEST_F(OrderManagerTest, LargeNumberTrades) {
    OrderManager manager(1000000.0, equity_path_, tradelog_path_);
    
    // Test with large quantities, but only within available cash
    manager.execute_buy("AAPL", 5000, 150.0); // 750,000
    manager.execute_buy("GOOG", 100, 2500.0); // 250,000
    
    EXPECT_EQ(manager.get_cash(), 0.0);
    EXPECT_EQ(manager.get_position("AAPL"), 5000);
    EXPECT_EQ(manager.get_position("GOOG"), 100);
}

TEST_F(OrderManagerTest, FileOutputFormat) {
    {
        OrderManager manager(100000.0, equity_path_, tradelog_path_);
        // Make some trades
        manager.execute_buy("AAPL", 100, 150.0);
        manager.execute_sell("GOOG", 50, 2500.0);
        // Record equity
        std::map<std::string, double> market_prices = {
            {"AAPL", 160.0},
            {"GOOG", 2600.0}
        };
        manager.record_equity(1234567890, market_prices);
    }
    // Check equity file format
    std::ifstream equity_file(equity_path_);
    std::string header;
    std::getline(equity_file, header);
    EXPECT_EQ(header, "timestamp,equity");
    std::string data_line;
    std::getline(equity_file, data_line);
    EXPECT_FALSE(data_line.empty());
    // Check tradelog file format
    std::ifstream tradelog_file(tradelog_path_);
    std::string tradelog_header;
    std::getline(tradelog_file, tradelog_header);
    EXPECT_EQ(tradelog_header, "timestamp,symbol,type,quantity,price,cash");
    std::string trade_line;
    std::getline(tradelog_file, trade_line);
    EXPECT_FALSE(trade_line.empty());
}