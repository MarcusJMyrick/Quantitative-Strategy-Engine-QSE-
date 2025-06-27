#include "qse/factor/MultiFactorCalculator.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <input_csv> <output_parquet> <weights_yaml>" << std::endl;
        std::cout << "Example: " << argv[0] << " data/daily_prices_AAPL.csv factors_AAPL.parquet config/factor_weights.yaml" << std::endl;
        return 1;
    }

    std::string input_csv = argv[1];
    std::string output_parquet = argv[2];
    std::string weights_yaml = argv[3];

    try {
        qse::MultiFactorCalculator calculator;
        calculator.compute_factors(input_csv, output_parquet, weights_yaml);
        std::cout << "Successfully computed factors and saved to: " << output_parquet << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 