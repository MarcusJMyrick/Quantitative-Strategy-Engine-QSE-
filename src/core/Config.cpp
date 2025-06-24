#include "qse/core/Config.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <stdexcept>

namespace qse {

bool Config::load_config(const std::string& config_path) {
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        
        // Load symbol slippage coefficients
        if (config["symbols"]) {
            auto symbols = config["symbols"];
            for (const auto& symbol_node : symbols) {
                std::string symbol = symbol_node.first.as<std::string>();
                auto slippage_node = symbol_node.second["slippage"];
                if (slippage_node && slippage_node["linear_coeff"]) {
                    double coeff = slippage_node["linear_coeff"].as<double>();
                    linear_impact_[symbol] = coeff;
                    std::cout << "Loaded slippage coefficient for " << symbol 
                              << ": " << coeff << std::endl;
                }
            }
        }
        
        // Load backtester settings
        if (config["backtester"]) {
            auto backtester = config["backtester"];
            if (backtester["initial_cash"]) {
                initial_cash_ = backtester["initial_cash"].as<double>();
            }
            if (backtester["commission_rate"]) {
                commission_rate_ = backtester["commission_rate"].as<double>();
            }
            if (backtester["min_trade_size"]) {
                min_trade_size_ = backtester["min_trade_size"].as<int>();
            }
        }
        
        // Load data paths
        if (config["data"]) {
            auto data = config["data"];
            if (data["base_path"]) {
                data_base_path_ = data["base_path"].as<std::string>();
            }
            if (data["processed_path"]) {
                processed_data_path_ = data["processed_path"].as<std::string>();
            }
            if (data["results_path"]) {
                results_path_ = data["results_path"].as<std::string>();
            }
        }
        
        std::cout << "Configuration loaded successfully from: " << config_path << std::endl;
        return true;
        
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading YAML config: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

double Config::get_slippage_coeff(const std::string& symbol) const {
    auto it = linear_impact_.find(symbol);
    if (it != linear_impact_.end()) {
        return it->second;
    }
    return 0.0; // Default to no slippage if symbol not found
}

} // namespace qse 