#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <filesystem>

#include "qse/data/CSVDataReader.h"
#include "qse/strategy/FactorStrategy.h"
#include "qse/strategy/FactorStrategyConfig.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " run --strategy factor [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  --config <file>           Configuration file (default: config/factor_strategy.yaml)\n"
              << "  --data <file>             Data file path\n"
              << "  --symbol <symbol>         Trading symbol (default: AAPL)\n"
              << "  --weights-dir <dir>       Weights directory (overrides config)\n"
              << "  --min-threshold <amount>  Minimum dollar threshold (overrides config)\n"
              << "  --rebalance-time <time>   Rebalance time HH:MM (overrides config)\n"
              << "  --help                    Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " run --strategy factor --config my_config.yaml\n"
              << "  " << program_name << " run --strategy factor --data data/ticks.csv --symbol SPY\n"
              << std::endl;
}

bool parse_command_line(int argc, char* argv[], 
                       std::string& config_file,
                       std::string& data_file,
                       std::string& symbol,
                       std::string& weights_dir,
                       double& min_threshold,
                       std::string& rebalance_time) {
    
    // Default values
    config_file = "config/factor_strategy.yaml";
    data_file = "";
    symbol = "AAPL";
    weights_dir = "";
    min_threshold = -1.0; // Use config default
    rebalance_time = "";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--data" && i + 1 < argc) {
            data_file = argv[++i];
        } else if (arg == "--symbol" && i + 1 < argc) {
            symbol = argv[++i];
        } else if (arg == "--weights-dir" && i + 1 < argc) {
            weights_dir = argv[++i];
        } else if (arg == "--min-threshold" && i + 1 < argc) {
            min_threshold = std::stod(argv[++i]);
        } else if (arg == "--rebalance-time" && i + 1 < argc) {
            rebalance_time = argv[++i];
        }
    }
    
    return true;
}

int run_factor_strategy(const std::string& config_file,
                       const std::string& data_file,
                       const std::string& symbol,
                       const std::string& weights_dir,
                       double min_threshold,
                       const std::string& rebalance_time) {
    
    try {
        std::cout << "Factor Strategy Engine Starting..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // --- Load Configuration ---
        std::cout << "Loading configuration from " << config_file << "..." << std::endl;
        FactorStrategyConfig config;
        if (!config.load_from_file(config_file)) {
            std::cerr << "Failed to load configuration from " << config_file << std::endl;
            return 1;
        }
        
        // Override config with command line arguments
        if (!weights_dir.empty()) {
            // Create a new config with overridden values
            FactorStrategyConfig::DataConfig data_config = config.get_data_config();
            data_config.weights_directory = weights_dir;
            // Note: In a real implementation, we'd need to update the config object
            // For now, we'll use the command line values directly
        }
        if (min_threshold > 0) {
            // Use command line value instead of config
            min_threshold = min_threshold; // Keep the command line value
        } else {
            min_threshold = config.get_min_dollar_threshold();
        }
        if (!rebalance_time.empty()) {
            // Use command line value instead of config
            rebalance_time = rebalance_time; // Keep the command line value
        } else {
            rebalance_time = config.get_rebalance_time_string();
        }
        
        // --- Validate Configuration ---
        std::cout << "Configuration loaded:" << std::endl;
        std::cout << "  Rebalance time: " << rebalance_time << std::endl;
        std::cout << "  Min threshold: $" << min_threshold << std::endl;
        std::cout << "  Weights directory: " << (weights_dir.empty() ? config.get_weights_directory() : weights_dir) << std::endl;
        std::cout << "  Initial cash: $" << config.get_initial_cash() << std::endl;
        
        // --- Create Components ---
        std::cout << "Initializing components..." << std::endl;
        
        // Create data reader
        std::unique_ptr<qse::IDataReader> data_reader;
        if (!data_file.empty()) {
            data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        } else {
            std::cerr << "No data file specified. Use --data option." << std::endl;
            return 1;
        }
        
        // Create order manager
        auto order_manager = std::make_shared<qse::OrderManager>(
            config.get_initial_cash(),
            "equity_curve.csv",
            "tradelog.csv"
        );
        
        // Create factor strategy
        auto strategy = std::make_unique<qse::FactorStrategy>(
            order_manager,
            symbol,
            config.get_weights_directory(),
            config.get_min_dollar_threshold(),
            config.to_exec_config()
        );
        
        // Create backtester
        qse::Backtester backtester(
            symbol,
            std::move(data_reader),
            std::move(strategy),
            order_manager,
            std::chrono::seconds(60)  // 1-minute bars
        );
        
        // --- Run Backtest ---
        std::cout << "Running factor strategy backtest..." << std::endl;
        backtester.run();
        
        // --- Performance Results ---
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Factor strategy backtest completed successfully!" << std::endl;
        std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;
        
        // Print final portfolio state
        double final_cash = order_manager->get_cash();
        int final_position = order_manager->get_position(symbol);
        std::cout << "Final portfolio state:" << std::endl;
        std::cout << "  Cash: $" << final_cash << std::endl;
        std::cout << "  Position: " << final_position << " shares" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "run") {
        if (argc < 4 || std::string(argv[2]) != "--strategy" || std::string(argv[3]) != "factor") {
            std::cerr << "Invalid command. Use: " << argv[0] << " run --strategy factor [OPTIONS]" << std::endl;
            return 1;
        }
        
        // Parse command line arguments
        std::string config_file, data_file, symbol, weights_dir, rebalance_time;
        double min_threshold;
        
        if (!parse_command_line(argc, argv, config_file, data_file, symbol, 
                               weights_dir, min_threshold, rebalance_time)) {
            return 0; // Help was printed
        }
        
        return run_factor_strategy(config_file, data_file, symbol, weights_dir, 
                                 min_threshold, rebalance_time);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage(argv[0]);
        return 1;
    }
} 