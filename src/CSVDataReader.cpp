#include "CSVDataReader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace qse {

CSVDataReader::CSVDataReader(const std::string& file_path) : file_path_(file_path) {
    load_data();
}

// The main loading function now determines whether to load ticks or bars
// based on the file content. This is a simple approach for now.
void CSVDataReader::load_data() {
    std::ifstream file(file_path_);
    std::string line;
    if (!file.is_open()) {
        throw std::runtime_error("File not found: " + file_path_);
    }

    // Read header to determine file type
    std::getline(file, line);
    std::stringstream header_ss(line);
    std::string first_col;
    std::getline(header_ss, first_col, ',');

    // Reset file to the beginning to re-read header in the loop
    file.clear();
    file.seekg(0, std::ios::beg);
    std::getline(file, line); // Skip header again

    // Simple check: if the first column is "timestamp", assume it's a bar file.
    // Otherwise, assume it's a tick file (e.g., from a different source).
    // A more robust implementation would have explicit readers for each format.
    if (first_col == "timestamp") { // Assuming this is our bar file format
         while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
            while(std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if(tokens.size() >= 6) {
                Bar bar;
                bar.timestamp = Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                bar.open = std::stod(tokens[1]);
                bar.high = std::stod(tokens[2]);
                bar.low = std::stod(tokens[3]);
                bar.close = std::stod(tokens[4]);
                bar.volume = std::stoll(tokens[5]);
                bars_.push_back(bar);
            }
        }
    } else { // Assume it's a tick file with format: timestamp,price,volume
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
             while(std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if(tokens.size() >= 3) {
                Tick tick;
                tick.timestamp = Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                tick.price = std::stod(tokens[1]);
                tick.volume = std::stoll(tokens[2]);
                ticks_.push_back(tick);
            }
        }
    }
}

const std::vector<qse::Tick>& CSVDataReader::read_all_ticks() {
    return ticks_;
}

const std::vector<qse::Bar>& CSVDataReader::read_all_bars() {
    // This is now less used, but we keep it for backward compatibility and testing.
    return bars_;
}

} // namespace qse