#include "CSVDataReader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <chrono>

namespace qse {

CSVDataReader::CSVDataReader(const std::string& file_path)
    : file_path_(file_path) {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File not found: " + file_path);
    }
    load_bars();
}

void CSVDataReader::load_bars() {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path_);
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        Bar bar;

        // Parse timestamp
        std::getline(ss, value, ',');
        using namespace std::chrono;
        bar.timestamp = system_clock::time_point{seconds(std::stoll(value))};

        // Parse open
        std::getline(ss, value, ',');
        bar.open = std::stod(value);

        // Parse high
        std::getline(ss, value, ',');
        bar.high = std::stod(value);

        // Parse low
        std::getline(ss, value, ',');
        bar.low = std::stod(value);

        // Parse close
        std::getline(ss, value, ',');
        bar.close = std::stod(value);

        // Parse volume
        std::getline(ss, value, ',');
        bar.volume = std::stoull(value);

        bars_.push_back(bar);
    }
}

std::vector<Bar> CSVDataReader::read_all_bars() {
    return bars_;
}

size_t CSVDataReader::get_bar_count() const {
    return bars_.size();
}

Bar CSVDataReader::get_bar(size_t index) const {
    if (index >= bars_.size()) {
        throw std::out_of_range("Index out of range");
    }
    return bars_[index];
}

std::vector<Bar> CSVDataReader::read_bars_in_range(Timestamp start_time, Timestamp end_time) {
    std::vector<Bar> result;
    for (const auto& bar : bars_) {
        if (bar.timestamp >= start_time && bar.timestamp <= end_time) {
            result.push_back(bar);
        }
    }
    return result;
}

} // namespace qse