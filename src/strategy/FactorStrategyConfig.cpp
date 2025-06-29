#include "qse/strategy/FactorStrategyConfig.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace qse {

bool FactorStrategyConfig::load_from_file(const std::string& config_path) {
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        return load_from_node(config);
    } catch (const YAML::Exception& e) {
        std::cerr << "[ERROR] Failed to load config file " << config_path 
                  << ": " << e.what() << std::endl;
        return false;
    }
}

bool FactorStrategyConfig::load_from_string(const std::string& yaml_content) {
    try {
        YAML::Node config = YAML::Load(yaml_content);
        return load_from_node(config);
    } catch (const YAML::Exception& e) {
        std::cerr << "[ERROR] Failed to parse YAML content: " << e.what() << std::endl;
        return false;
    }
}

bool FactorStrategyConfig::load_from_node(const YAML::Node& config) {
    // Load rebalance time
    if (config["rebalance_time"]) {
        rebalance_time_ = config["rebalance_time"].as<std::string>();
    }
    
    // Load min dollar threshold
    if (config["min_dollar_threshold"]) {
        min_dollar_threshold_ = config["min_dollar_threshold"].as<double>();
    }
    
    // Load engine configuration
    if (config["engine"]) {
        YAML::Node engine = config["engine"];
        if (engine["order_style"]) {
            engine_.order_style = engine["order_style"].as<std::string>();
        }
        if (engine["max_px_impact"]) {
            engine_.max_px_impact = engine["max_px_impact"].as<double>();
        }
        if (engine["min_notional"]) {
            engine_.min_notional = engine["min_notional"].as<double>();
        }
        if (engine["lot_size"]) {
            engine_.lot_size = engine["lot_size"].as<int>();
        }
        if (engine["min_qty"]) {
            engine_.min_qty = engine["min_qty"].as<double>();
        }
    }
    
    // Load portfolio configuration
    if (config["portfolio"]) {
        YAML::Node portfolio = config["portfolio"];
        if (portfolio["initial_cash"]) {
            portfolio_.initial_cash = portfolio["initial_cash"].as<double>();
        }
        if (portfolio["max_position_size"]) {
            portfolio_.max_position_size = portfolio["max_position_size"].as<double>();
        }
        if (portfolio["max_leverage"]) {
            portfolio_.max_leverage = portfolio["max_leverage"].as<double>();
        }
    }
    
    // Load data configuration
    if (config["data"]) {
        YAML::Node data = config["data"];
        if (data["weights_directory"]) {
            data_.weights_directory = data["weights_directory"].as<std::string>();
        }
        if (data["price_source"]) {
            data_.price_source = data["price_source"].as<std::string>();
        }
    }
    
    // Load logging configuration
    if (config["logging"]) {
        YAML::Node logging = config["logging"];
        if (logging["level"]) {
            logging_.level = logging["level"].as<std::string>();
        }
        if (logging["equity_curve"]) {
            logging_.equity_curve = logging["equity_curve"].as<bool>();
        }
        if (logging["trade_log"]) {
            logging_.trade_log = logging["trade_log"].as<bool>();
        }
        if (logging["performance"]) {
            logging_.performance = logging["performance"].as<bool>();
        }
    }
    
    return true;
}

std::chrono::minutes FactorStrategyConfig::get_rebalance_time_minutes() const {
    auto time_opt = parse_time_string(rebalance_time_);
    if (time_opt) {
        return *time_opt;
    }
    // Default to 15:45 if parsing fails
    return std::chrono::minutes(15 * 60 + 45);
}

std::string FactorStrategyConfig::get_rebalance_time_string() const {
    return rebalance_time_;
}

ExecConfig FactorStrategyConfig::to_exec_config() const {
    ExecConfig config;
    config.rebal_time = rebalance_time_;
    config.order_style = engine_.order_style;
    config.max_px_impact = engine_.max_px_impact;
    config.min_notional = engine_.min_notional;
    config.lot_size = engine_.lot_size;
    config.min_qty = engine_.min_qty;
    return config;
}

std::optional<std::chrono::minutes> FactorStrategyConfig::parse_time_string(const std::string& time_str) const {
    std::istringstream ss(time_str);
    int hours, minutes;
    char colon;
    
    if (ss >> hours >> colon >> minutes && colon == ':') {
        if (hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59) {
            return std::chrono::minutes(hours * 60 + minutes);
        }
    }
    
    std::cerr << "[WARN] Invalid time format: " << time_str 
              << ". Expected HH:MM format." << std::endl;
    return std::nullopt;
}

} // namespace qse 