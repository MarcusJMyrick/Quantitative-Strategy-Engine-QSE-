#pragma once

#include "qse/exe/FactorExecutionEngine.h"
#include <string>
#include <chrono>
#include <optional>

// Forward declaration to avoid heavy include in header
namespace YAML { class Node; }

namespace qse {

class FactorStrategyConfig {
public:
    struct EngineConfig {
        std::string order_style{"market"};
        double max_px_impact{0.01};
        double min_notional{100.0};
        int    lot_size{1};
        double min_qty{1.0};
    };

    struct PortfolioConfig {
        double initial_cash{1'000'000.0};
        double max_position_size{0.20};
        double max_leverage{1.5};
    };

    struct DataConfig {
        std::string weights_directory{"data/weights"};
        std::string price_source{"close"};
    };

    struct LoggingConfig {
        std::string level{"info"};
        bool equity_curve{true};
        bool trade_log{true};
        bool performance{true};
    };

    // ----------------- API -----------------
    bool load_from_file(const std::string& path);
    bool load_from_string(const std::string& yaml_text);

    std::chrono::minutes get_rebalance_time_minutes() const;
    std::string          get_rebalance_time_string() const;

    ExecConfig to_exec_config() const;

    // section accessors
    const EngineConfig&    get_engine_config()    const { return engine_; }
    const PortfolioConfig& get_portfolio_config() const { return portfolio_; }
    const DataConfig&      get_data_config()      const { return data_; }
    const LoggingConfig&   get_logging_config()   const { return logging_; }

    // scalar helpers
    double       get_min_dollar_threshold() const { return min_dollar_threshold_; }
    double       get_initial_cash()         const { return portfolio_.initial_cash; }
    std::string  get_weights_directory()    const { return data_.weights_directory; }

private:
    // YAML helpers
    bool load_from_node(const YAML::Node& node);
    std::optional<std::chrono::minutes> parse_time_string(const std::string& str) const;

    // ---------- stored values ----------
    std::string rebalance_time_{"15:45"};
    double      min_dollar_threshold_{50.0};

    EngineConfig    engine_{};
    PortfolioConfig portfolio_{};
    DataConfig      data_{};
    LoggingConfig   logging_{};
};

} // namespace qse
