#include "qse/data/CSVDataReader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

namespace qse {

CSVDataReader::CSVDataReader(const std::string& file_path) : file_path_(file_path) {
    load_data();
}

void CSVDataReader::load_data() {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path_);
    }

    std::string header_line;
    if (!std::getline(file, header_line)) {
        throw std::runtime_error("Cannot read header from file: " + file_path_);
    }

    // --- SUGGESTION: Detect file type based on column count ---
    std::stringstream header_ss(header_line);
    std::string column;
    std::vector<std::string> header_columns;
    while (std::getline(header_ss, column, ',')) {
        header_columns.push_back(column);
    }

    std::string line;
    if (header_columns.size() >= 6) { // Assume Bar data
        std::cout << "Detected Bar data format in " << file_path_ << std::endl;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
            while(std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if(tokens.size() >= 6) {
                Bar bar;
                // Note: Using qse::Timestamp for clarity
                bar.symbol = "UNKNOWN"; // Default symbol for CSV files
                bar.timestamp = qse::Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                bar.open = std::stod(tokens[1]);
                bar.high = std::stod(tokens[2]);
                bar.low = std::stod(tokens[3]);
                bar.close = std::stod(tokens[4]);
                bar.volume = std::stoull(tokens[5]); // Use stoull for unsigned long long
                bars_.push_back(bar);
            }
        }
    } else if (header_columns.size() >= 3) { // Assume Tick data
        std::cout << "Detected Tick data format in " << file_path_ << std::endl;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
             while(std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if(tokens.size() >= 3) {
                Tick tick;
                tick.timestamp = qse::Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                tick.price = std::stod(tokens[1]);
                tick.volume = std::stoull(tokens[2]); // Use stoull for unsigned long long
                ticks_.push_back(tick);
            }
        }
    } else {
        throw std::runtime_error("Unknown file format in: " + file_path_);
    }
}

// Return by const reference, as per the interface
const std::vector<qse::Tick>& CSVDataReader::read_all_ticks() const {
    return ticks_;
}

// Return by const reference, as per the interface
const std::vector<qse::Bar>& CSVDataReader::read_all_bars() const {
    return bars_;
}

} // namespace qse