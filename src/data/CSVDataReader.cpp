#include "qse/data/CSVDataReader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

namespace qse {

CSVDataReader::CSVDataReader(const std::string& file_path)
    : file_path_(file_path), symbol_override_("") {
    load_data();
}

CSVDataReader::CSVDataReader(const std::string& file_path, const std::string& symbol_override)
    : file_path_(file_path), symbol_override_(symbol_override) {
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

    // Detect file type based on header content
    bool isBar = (header_line.find("Open") != std::string::npos || 
                  header_line.find("open") != std::string::npos);
    
    std::string line;
    if (isBar) {
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
                bar.symbol = symbol_override_.empty() ? "UNKNOWN" : symbol_override_;
                bar.timestamp = qse::Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                bar.open = std::stod(tokens[1]);
                bar.high = std::stod(tokens[2]);
                bar.low = std::stod(tokens[3]);
                bar.close = std::stod(tokens[4]);
                bar.volume = std::stoull(tokens[5]);
                bars_.push_back(bar);
            }
        }
    } else {
        std::cout << "Detected Tick data format in " << file_path_ << std::endl;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
            while(std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if(tokens.size() >= 8) {
                // Full tick format: timestamp,symbol,price,volume,bid,ask,bid_size,ask_size
                Tick tick;
                {
                    long long raw = std::stoll(tokens[0]);
                    if (raw < 10'000'000'000LL) {
                        raw *= 1000; // CSV provides seconds – promote to ms
                    }
                    tick.timestamp = qse::Timestamp(std::chrono::milliseconds(raw));
                }
                tick.symbol = symbol_override_.empty() ? tokens[1] : symbol_override_;
                tick.price = std::stod(tokens[2]);
                tick.volume = std::stoull(tokens[3]);
                tick.bid = std::stod(tokens[4]);
                tick.ask = std::stod(tokens[5]);
                tick.bid_size = std::stoull(tokens[6]);
                tick.ask_size = std::stoull(tokens[7]);
                ticks_.push_back(tick);
            } else if(tokens.size() >= 3) {
                // Legacy format: timestamp,price,volume
                Tick tick;
                {
                    long long raw = std::stoll(tokens[0]);
                    if (raw < 10'000'000'000LL) {
                        raw *= 1000;
                    }
                    tick.timestamp = qse::Timestamp(std::chrono::milliseconds(raw));
                }
                tick.symbol = symbol_override_.empty() ? "UNKNOWN" : symbol_override_;
                tick.price = std::stod(tokens[1]);
                tick.volume = std::stoull(tokens[2]);
                tick.bid = tick.price; // Use price as bid/ask for legacy format
                tick.ask = tick.price;
                tick.bid_size = tick.volume; // Use volume as size for legacy format
                tick.ask_size = tick.volume;
                ticks_.push_back(tick);
            }
        }
        
        // Sort ticks by timestamp
        std::sort(ticks_.begin(), ticks_.end(), 
            [](const Tick& a, const Tick& b) { return a.timestamp < b.timestamp; });
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