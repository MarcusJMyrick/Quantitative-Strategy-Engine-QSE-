#include <gtest/gtest.h>
#include "qse/exe/FactorExecutionEngine.h"
#include "qse/order/IOrderManager.h"
#include <memory>

using namespace qse;

// Simple Mock OrderManager for testing
class SimpleMockOrderManager : public IOrderManager {
public:
    std::vector<Position> get_positions() const override {
        return {{"AAPL", 100.0}};
    }
    
    // Stub implementations for other interface methods
    void execute_buy(const std::string&, int, double) override {}
    void execute_sell(const std::string&, int, double) override {}
    
    // Additional interface methods
    OrderId submit_market_order(const std::string&, Order::Side, Volume) override { return ""; }
    OrderId submit_limit_order(const std::string&, Order::Side, Volume, Price, Order::TimeInForce) override { return ""; }
    bool cancel_order(const OrderId&) override { return false; }
    void process_tick(const Tick&) override {}
    void attempt_fills() override {}
    void set_fill_callback(FillCallback) override {}
    std::optional<Order> get_order(const OrderId&) const override { return std::nullopt; }
    std::vector<Order> get_active_orders(const std::string&) const override { return {}; }
    int get_position(const std::string&) const override { return 0; }
    double get_cash() const override { return 0.0; }
    void record_equity(long long, const std::map<std::string, double>&) override {}
};

TEST(HoldingsSnapshotTest, EmptyOK) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    
    auto holdings = engine.fetch_holdings();
    EXPECT_TRUE(holdings.empty());
}

TEST(HoldingsSnapshotTest, SimpleWithPositions) {
    ExecConfig cfg;
    auto mock_om = std::make_shared<SimpleMockOrderManager>();
    FactorExecutionEngine engine(cfg, mock_om);
    
    auto holdings = engine.fetch_holdings();
    
    EXPECT_EQ(holdings.size(), 1);
    EXPECT_EQ(holdings["AAPL"], 100.0);
}

TEST(HoldingsSnapshotTest, NoOrderManager) {
    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    auto holdings = engine.fetch_holdings();
    
    EXPECT_TRUE(holdings.empty());
}
