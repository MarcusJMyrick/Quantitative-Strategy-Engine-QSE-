#include "qse/exe/FactorExecutionEngine.h"
#include "qse/data/Data.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <fstream>

using namespace qse;

// Dummy order manager that just prints orders
class PrintOrderManager : public IOrderManager {
public:
    OrderId submit_market_order(const std::string& symbol, Order::Side side, Volume quantity) override {
        std::cerr << "[ORDER] " << symbol << " " << (side == Order::Side::BUY ? "BUY" : "SELL") << " " << quantity << " (MARKET)\n";
        return symbol + "_order";
    }
    OrderId submit_limit_order(const std::string&, Order::Side, Volume, Price, Order::TimeInForce) override { return ""; }
    bool cancel_order(const OrderId&) override { return false; }
    void process_tick(const Tick&) override {}
    void attempt_fills() override {}
    void set_fill_callback(FillCallback) override {}
    std::optional<Order> get_order(const OrderId&) const override { return std::nullopt; }
    std::vector<Order> get_active_orders(const std::string&) const override { return {}; }
    void execute_buy(const std::string&, int, double) override {}
    void execute_sell(const std::string&, int, double) override {}
    int get_position(const std::string&) const override { return 0; }
    std::vector<Position> get_positions() const override { return {}; }
    double get_cash() const override { return 1000000.0; }
    void record_equity(long long, const std::map<std::string, double>&) override {}
};

int main(int argc, char* argv[]) {
    std::cerr << "[DEMO] Factor Execution Demo starting..." << std::endl;
    try {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <weights.csv> <prices.csv>\n";
            return 1;
        }
        std::string weights_file = argv[1];
        std::string prices_file = argv[2];

        ExecConfig cfg;
        cfg.order_style = "market";
        cfg.min_qty = 0;
        cfg.lot_size = 1;
        cfg.rebal_time = "15:45";

        auto om = std::make_shared<PrintOrderManager>();
        FactorExecutionEngine engine(cfg, om);

        // Load weights
        auto weights = engine.load_weights(weights_file);
        std::cerr << "Loaded weights:" << std::endl;
        for (const auto& w : weights) {
            std::cerr << w.first << ": " << w.second << std::endl;
        }
        std::cerr << std::flush;

        // Dummy holdings: empty
        std::unordered_map<std::string, double> holdings;
        double cash = 1000000.0;

        // Load prices from CSV: symbol,price per line
        std::unordered_map<std::string, double> prices;
        {
            std::ifstream ifs(prices_file);
            if (!ifs.is_open()) {
                throw std::runtime_error("Could not open prices file: " + prices_file);
            }
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.empty()) continue;
                size_t comma = line.find(',');
                if (comma == std::string::npos) continue;
                std::string sym = line.substr(0, comma);
                double px = std::stod(line.substr(comma + 1));
                prices[sym] = px;
            }
        }
        std::cerr << "Loaded prices:" << std::endl;
        for (const auto& p : prices) {
            std::cerr << p.first << ": " << p.second << std::endl;
        }
        std::cerr << std::flush;

        // Calculate target shares
        auto target_shares = engine.calc_target_shares(weights, holdings, cash, prices);
        std::cerr << "Target shares:" << std::endl;
        for (const auto& ts : target_shares) {
            std::cerr << ts.first << ": " << ts.second << std::endl;
        }
        std::cerr << std::flush;

        // Build orders
        auto orders = engine.build_orders(target_shares, weights);
        std::cerr << "Built orders:" << std::endl;
        for (const auto& order : orders) {
            std::cerr << order.symbol << " " << (order.side == Order::Side::BUY ? "BUY" : "SELL")
                      << " " << order.quantity << " type=";
            if (order.type == Order::Type::MARKET) std::cerr << "MARKET";
            else if (order.type == Order::Type::TARGET_PERCENT) std::cerr << "TARGET_PERCENT";
            else if (order.type == Order::Type::LIMIT) std::cerr << "LIMIT";
            else std::cerr << "OTHER";
            std::cerr << std::endl;
        }
        std::cerr << std::flush;

        // Submit orders (prints via PrintOrderManager)
        engine.submit_orders(orders);

        std::cerr << "[DEMO] Completed." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[DEMO ERROR] " << ex.what() << std::endl;
        return 1;
    }
    return 0;
} 