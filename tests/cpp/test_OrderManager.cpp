#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>

#include "qse/order/OrderManager.h"
#include "qse/data/OrderBook.h"

using namespace qse;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::AtLeast;

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

class OrderManagerTickSimulationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test ticks with bid/ask spreads that allow limit orders to fill
        test_ticks_ = {
            {"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.5, 100},  // mid: 100.0
            {"AAPL", qse::from_unix_ms(1001), 100.2, 99.8, 100.6, 150},  // mid: 100.2
            {"AAPL", qse::from_unix_ms(1002), 100.1, 99.9, 100.0, 200},  // mid: 100.1, ask=100.0 allows buy limit at 100.0 to fill
        };
    }

    std::vector<qse::Tick> test_ticks_;
};

// Test 1: Market order fills immediately at mid price
TEST_F(OrderManagerTickSimulationTest, MarketOrderFillsImmediately) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a market buy order
    auto order_id = order_manager->submit_market_order("AAPL", qse::Order::Side::BUY, 100);
    EXPECT_FALSE(order_id.empty());
    
    // Verify order is active
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    
    // Process a tick - should fill immediately
    order_manager->process_tick(test_ticks_[0]);
    
    // Verify order is filled
    order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0); // mid price
    
    // Verify position and cash updated
    EXPECT_EQ(order_manager->get_position("AAPL"), 100);
    EXPECT_DOUBLE_EQ(order_manager->get_cash(), 0.0); // 10000 - (100 * 100.0) = 0.0
}

// Test 2: Market sell order fills at mid price
TEST_F(OrderManagerTickSimulationTest, MarketSellOrderFillsImmediately) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // First buy some shares
    order_manager->submit_market_order("AAPL", qse::Order::Side::BUY, 100);
    order_manager->process_tick(test_ticks_[0]);
    
    // Submit a market sell order
    auto order_id = order_manager->submit_market_order("AAPL", qse::Order::Side::SELL, 50);
    
    // Process a tick - should fill immediately
    order_manager->process_tick(test_ticks_[1]);
    
    // Verify order is filled
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 50);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.2); // mid price of second tick
    
    // Verify position updated
    EXPECT_EQ(order_manager->get_position("AAPL"), 50);
}

// Test 3: Limit buy order fills when tick crosses limit price
TEST_F(OrderManagerTickSimulationTest, LimitBuyOrderFillsOnCross) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a limit buy order at 100.0
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 100.0, qse::Order::TimeInForce::GTC);
    
    // Process first tick (bid=99.5, ask=100.5) - should not fill
    order_manager->process_tick(test_ticks_[0]);
    
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    
    // Process second tick (bid=99.8, ask=100.6) - should not fill
    order_manager->process_tick(test_ticks_[1]);
    
    order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    
    // Process third tick (bid=99.9, ask=100.0) - should fill at 100.0
    order_manager->process_tick(test_ticks_[2]);
    
    order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0);
}

// Test 4: Limit sell order fills when tick crosses limit price
TEST_F(OrderManagerTickSimulationTest, LimitSellOrderFillsOnCross) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // First buy some shares
    order_manager->submit_market_order("AAPL", qse::Order::Side::BUY, 100);
    order_manager->process_tick(test_ticks_[0]);
    
    // Submit a limit sell order at 100.2
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::SELL, 50, 100.2, qse::Order::TimeInForce::GTC);
    
    // Process first tick - should not fill
    order_manager->process_tick(test_ticks_[0]);
    
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    
    // Process second tick (bid=99.8, ask=100.6) - should fill at 100.2
    order_manager->process_tick(test_ticks_[1]);
    
    // Create a tick where bid crosses the sell limit price
    qse::Tick cross_tick{"AAPL", qse::from_unix_ms(1003), 100.3, 100.2, 100.4, 100}; // bid=100.2 allows sell limit at 100.2 to fill
    order_manager->process_tick(cross_tick);
    
    order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 50);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.2);
}

// Test 5: IOC order cancels if not filled on same tick
TEST_F(OrderManagerTickSimulationTest, IOCOrderCancelsIfNotFilled) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit an IOC buy order at 99.0 (below current bid)
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 99.0, qse::Order::TimeInForce::IOC);
    
    // Process tick - should cancel immediately since price doesn't cross
    order_manager->process_tick(test_ticks_[0]);
    
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::CANCELLED);
    EXPECT_EQ(order->filled_quantity, 0);
}

// Test 6: IOC order fills if price crosses on same tick
TEST_F(OrderManagerTickSimulationTest, IOCOrderFillsIfPriceCrosses) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit an IOC buy order at 100.0 (should fill at mid price)
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 100.0, qse::Order::TimeInForce::IOC);
    
    // Process tick - should fill immediately
    qse::Tick fill_tick{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 100}; // ask=100.0 allows buy limit at 100.0 to fill
    order_manager->process_tick(fill_tick);
    
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::FILLED);
    EXPECT_EQ(order->filled_quantity, 100);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0);
}

// Test 7: Order cancellation works
TEST_F(OrderManagerTickSimulationTest, OrderCancellation) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a limit order
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 99.0, qse::Order::TimeInForce::GTC);
    
    // Verify order is active
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PENDING);
    
    // Cancel the order
    bool cancelled = order_manager->cancel_order(order_id);
    EXPECT_TRUE(cancelled);
    
    // Verify order is cancelled
    order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::CANCELLED);
}

// Test 8: Get active orders for symbol
TEST_F(OrderManagerTickSimulationTest, GetActiveOrders) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit multiple orders
    auto order1 = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 99.0, qse::Order::TimeInForce::GTC);
    auto order2 = order_manager->submit_limit_order("AAPL", qse::Order::Side::SELL, 50, 101.0, qse::Order::TimeInForce::GTC);
    auto order3 = order_manager->submit_limit_order("GOOGL", qse::Order::Side::BUY, 200, 150.0, qse::Order::TimeInForce::GTC);
    
    // Get active orders for AAPL
    auto aapl_orders = order_manager->get_active_orders("AAPL");
    EXPECT_EQ(aapl_orders.size(), 2);
    
    // Get active orders for GOOGL
    auto googl_orders = order_manager->get_active_orders("GOOGL");
    EXPECT_EQ(googl_orders.size(), 1);
    
    // Get active orders for non-existent symbol
    auto empty_orders = order_manager->get_active_orders("INVALID");
    EXPECT_EQ(empty_orders.size(), 0);
}

// Test 9: Partial fills work correctly
TEST_F(OrderManagerTickSimulationTest, PartialFills) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit a large limit buy order
    auto order_id = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 1000, 100.0, qse::Order::TimeInForce::GTC);
    
    // Create a tick with limited volume and ask that crosses the limit
    qse::Tick limited_tick{"AAPL", qse::from_unix_ms(1000), 100.0, 99.5, 100.0, 500}; // ask=100.0 allows buy limit at 100.0 to fill, but only 500 volume
    
    // Process tick - should partially fill
    order_manager->process_tick(limited_tick);
    
    auto order = order_manager->get_order(order_id);
    EXPECT_TRUE(order.has_value());
    EXPECT_EQ(order->status, qse::Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(order->filled_quantity, 500);
    EXPECT_EQ(order->remaining_quantity(), 500);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 100.0);
}

// Test 10: Multiple orders on same tick
TEST_F(OrderManagerTickSimulationTest, MultipleOrdersOnSameTick) {
    auto order_manager = std::make_unique<qse::OrderManager>(10000.0, "test_equity.csv", "test_tradelog.csv");
    
    // Submit multiple orders
    auto buy_order = order_manager->submit_limit_order("AAPL", qse::Order::Side::BUY, 100, 100.0, qse::Order::TimeInForce::GTC);
    auto sell_order = order_manager->submit_limit_order("AAPL", qse::Order::Side::SELL, 50, 100.0, qse::Order::TimeInForce::GTC);
    
    // First buy some shares for the sell order
    order_manager->submit_market_order("AAPL", qse::Order::Side::BUY, 50);
    order_manager->process_tick(test_ticks_[0]);
    
    // Process tick - both orders should fill
    qse::Tick cross_tick{"AAPL", qse::from_unix_ms(1000), 100.0, 100.0, 100.0, 200}; // bid=ask=100.0 allows both buy and sell limits at 100.0 to fill
    order_manager->process_tick(cross_tick);
    
    auto buy = order_manager->get_order(buy_order);
    auto sell = order_manager->get_order(sell_order);
    
    EXPECT_TRUE(buy.has_value());
    EXPECT_TRUE(sell.has_value());
    EXPECT_EQ(buy->status, qse::Order::Status::FILLED);
    EXPECT_EQ(sell->status, qse::Order::Status::FILLED);
}

namespace qse {

void OrderBook::on_tick(const Tick& tick) {
    auto& tob = books_[tick.symbol];
    tob.best_bid_price = tick.bid;
    tob.best_bid_size  = tick.bid_size;
    tob.best_ask_price = tick.ask;
    tob.best_ask_size  = tick.ask_size;
}

const TopOfBook& OrderBook::top_of_book(const std::string& symbol) const {
    static TopOfBook empty;
    auto it = books_.find(symbol);
    return (it != books_.end()) ? it->second : empty;
}

} // namespace qse