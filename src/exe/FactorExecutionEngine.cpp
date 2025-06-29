#include "qse/exe/FactorExecutionEngine.h"

#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/api.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace qse {

using WeightMap = std::unordered_map<std::string, double>;

static WeightMap parse_csv(const std::string& path) {
    WeightMap out;
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open weight file: " + path);
    }
    std::string header;
    if (!std::getline(ifs, header)) {
        throw std::runtime_error("Empty weight file: " + path);
    }
    // Expect first two columns to be symbol and weight (case insensitive)
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string sym, wstr;
        if (!std::getline(ss, sym, ',')) continue;
        if (!std::getline(ss, wstr, ',')) {
            throw std::runtime_error("Malformed weights line: " + line);
        }
        double w = std::stod(wstr);
        if (std::isnan(w) || !std::isfinite(w)) {
            throw std::runtime_error("NaN/Inf weight for symbol " + sym);
        }
        out[sym] = w;
    }
    return out;
}

static WeightMap parse_parquet(const std::string& path) {
    WeightMap out;
    std::shared_ptr<arrow::io::ReadableFile> infile;
    auto res = arrow::io::ReadableFile::Open(path);
    if (!res.ok()) {
        throw std::runtime_error(res.status().ToString());
    }
    infile = *res;

    std::unique_ptr<parquet::arrow::FileReader> reader;
    auto st = parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    if (!st.ok()) {
        throw std::runtime_error(st.ToString());
    }
    std::shared_ptr<arrow::Table> table;
    st = reader->ReadTable(&table);
    if (!st.ok()) {
        throw std::runtime_error(st.ToString());
    }

    auto sym_col = table->GetColumnByName("symbol");
    auto w_col = table->GetColumnByName("weight");
    if (!sym_col || !w_col) {
        throw std::runtime_error("Parquet weights file missing required columns");
    }

    for (int chunk = 0; chunk < sym_col->num_chunks(); ++chunk) {
        auto sym_arr = std::static_pointer_cast<arrow::StringArray>(sym_col->chunk(chunk));
        auto w_arr = std::static_pointer_cast<arrow::DoubleArray>(w_col->chunk(chunk));
        int64_t n = sym_arr->length();
        for (int64_t i = 0; i < n; ++i) {
            if (!sym_arr->IsValid(i) || !w_arr->IsValid(i)) {
                throw std::runtime_error("Null value in weights parquet file");
            }
            std::string sym = sym_arr->GetString(i);
            double w = w_arr->Value(i);
            if (std::isnan(w) || !std::isfinite(w)) {
                throw std::runtime_error("NaN/Inf weight for symbol " + sym);
            }
            out[sym] = w;
        }
    }
    return out;
}

WeightMap FactorExecutionEngine::load_weights(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".csv") {
        return parse_csv(path);
    } else if (ext == ".parquet" || ext == ".pq") {
        return parse_parquet(path);
    }
    throw std::runtime_error("Unsupported weight file extension: " + ext);
}

std::unordered_map<std::string, double> FactorExecutionEngine::fetch_holdings() {
    std::unordered_map<std::string, double> holdings;
    if (!order_manager_) {
        return holdings;  // Return empty map if no order manager
    }
    
    // Get all positions from the order manager
    auto positions = order_manager_->get_positions();
    for (const auto& pos : positions) {
        holdings[pos.symbol] = pos.quantity;
    }
    
    return holdings;
}

std::unordered_map<std::string, long long> FactorExecutionEngine::calc_target_shares(
    const std::unordered_map<std::string, double>& target_weights,
    const std::unordered_map<std::string, double>& current_holdings,
    double cash,
    const std::unordered_map<std::string, double>& prices) {
    std::unordered_map<std::string, long long> target_shares;
    
    // Calculate total portfolio value (NAV)
    double nav = cash;
    for (const auto& holding : current_holdings) {
        const std::string& symbol = holding.first;
        double quantity = holding.second;
        auto price_it = prices.find(symbol);
        if (price_it != prices.end()) {
            nav += quantity * price_it->second;
        }
    }
    
    // Calculate target shares for each symbol
    for (const auto& weight_pair : target_weights) {
        const std::string& symbol = weight_pair.first;
        double target_weight = weight_pair.second;
        
        // Get current holdings (default to 0 if not found)
        double current_quantity = 0.0;
        auto holdings_it = current_holdings.find(symbol);
        if (holdings_it != current_holdings.end()) {
            current_quantity = holdings_it->second;
        }
        
        // Get current price (required for calculation)
        auto price_it = prices.find(symbol);
        if (price_it == prices.end()) {
            continue; // Skip symbols without price data
        }
        double price = price_it->second;
        
        if (price <= 0.0) {
            continue; // Skip symbols with invalid prices
        }
        
        // Calculate target quantity: target_weight * nav / price
        double target_quantity = target_weight * nav / price;
        
        // Overflow protection: skip if target quantity is unreasonably large
        // This prevents issues with penny stocks or extreme price scenarios
        const double MAX_REASONABLE_SHARES = 1e9;  // 1 billion shares max
        if (std::abs(target_quantity) > MAX_REASONABLE_SHARES) {
            continue; // Skip symbols that would require unreasonable share quantities
        }
        
        // Calculate difference: target - current
        double delta_quantity = target_quantity - current_quantity;
        
        // Round to nearest share (using lot_size from config)
        long long rounded_delta = std::round(delta_quantity / cfg_.lot_size) * cfg_.lot_size;
        
        // Only include if there's a meaningful difference
        if (std::abs(rounded_delta) > 0) {
            target_shares[symbol] = rounded_delta;
        }
    }
    
    return target_shares;
}

void FactorExecutionEngine::submit_orders(const std::vector<qse::Order>& orders) {
    if (!order_manager_) return;
    for (const auto& order : orders) {
        try {
            if (order.type == qse::Order::Type::MARKET) {
                order_manager_->submit_market_order(order.symbol, order.side, order.quantity);
            } else if (order.type == qse::Order::Type::TARGET_PERCENT) {
                // No support for target percent orders in IOrderManager
                // Log or throw
                fprintf(stderr, "[WARN] TARGET_PERCENT order for %s not supported by OrderManager.\n", order.symbol.c_str());
            } else {
                // Fallback: legacy path
                if (order.side == qse::Order::Side::BUY) {
                    order_manager_->execute_buy(order.symbol, order.quantity, order.limit_price);
                } else {
                    order_manager_->execute_sell(order.symbol, order.quantity, order.limit_price);
                }
            }
        } catch (const std::exception& ex) {
            fprintf(stderr, "[ERROR] Order submission failed for %s: %s\n", order.symbol.c_str(), ex.what());
        }
    }
}

bool FactorExecutionEngine::should_rebalance(std::chrono::system_clock::time_point now) const {
    // Parse rebal_time from HH:MM format
    std::string time_str = cfg_.rebal_time;
    size_t colon_pos = time_str.find(':');
    if (colon_pos == std::string::npos) {
        return false; // Invalid time format
    }
    
    int target_hour, target_minute;
    try {
        target_hour = std::stoi(time_str.substr(0, colon_pos));
        target_minute = std::stoi(time_str.substr(colon_pos + 1));
    } catch (const std::exception&) {
        return false; // Invalid time format
    }
    
    // Convert current time to local time
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    if (!tm) {
        return false;
    }
    
    // Check if current time matches rebalance time (within 1 minute tolerance)
    bool time_matches = (tm->tm_hour == target_hour && 
                        std::abs(tm->tm_min - target_minute) <= 1);
    
    if (!time_matches) {
        return false;
    }
    
    // Check if we've already rebalanced today
    std::tm today_tm = {};
    today_tm.tm_year = tm->tm_year;
    today_tm.tm_mon = tm->tm_mon;
    today_tm.tm_mday = tm->tm_mday;
    auto today_start = std::chrono::system_clock::from_time_t(std::mktime(&today_tm));
    
    if (last_rebalance_ >= today_start) {
        return false; // Already rebalanced today
    }
    
    // Update last rebalance time and allow rebalancing
    last_rebalance_ = now;
    return true;
}

std::vector<qse::Order> FactorExecutionEngine::build_orders(const std::unordered_map<std::string, long long>& target_qty, const std::unordered_map<std::string, double>& target_weights) {
    std::vector<qse::Order> orders;
    for (const auto& pair : target_qty) {
        const std::string& symbol = pair.first;
        long long qty = pair.second;
        if (std::abs(qty) <= cfg_.min_qty) continue;
        qse::Order order;
        order.symbol = symbol;
        order.quantity = std::abs(qty);
        order.side = qty > 0 ? qse::Order::Side::BUY : qse::Order::Side::SELL;
        order.status = qse::Order::Status::PENDING;
        order.filled_quantity = 0;
        order.avg_fill_price = 0.0;
        order.timestamp = std::chrono::system_clock::now();
        if (cfg_.order_style == "target_percent") {
            order.type = qse::Order::Type::TARGET_PERCENT;
            auto it = target_weights.find(symbol);
            order.target_percent = (it != target_weights.end()) ? it->second : 0.0;
        } else {
            order.type = qse::Order::Type::MARKET;
            order.target_percent = 0.0;
        }
        orders.push_back(order);
    }
    return orders;
}

} // namespace qse 