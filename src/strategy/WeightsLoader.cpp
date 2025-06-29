#include "qse/strategy/WeightsLoader.h"
#include <arrow/io/api.h>
#include <arrow/csv/api.h>
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/api.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace qse {

std::string WeightsLoader::date_to_string(const std::chrono::system_clock::time_point& date) {
    auto time_t = std::chrono::system_clock::to_time_t(date);
    std::tm* tm = std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(4) << (tm->tm_year + 1900)
        << std::setw(2) << (tm->tm_mon + 1)
        << std::setw(2) << tm->tm_mday;
    
    return oss.str();
}

std::string WeightsLoader::generate_filename(const std::string& base_path,
                                           const std::chrono::system_clock::time_point& date) {
    std::string date_str = date_to_string(date);
    return (std::filesystem::path(base_path) / ("weights_" + date_str + ".csv")).string();
}

std::optional<std::unordered_map<std::string, double>> 
WeightsLoader::load_weights_from_file(const std::string& file_path) {
    std::unordered_map<std::string, double> weights;
    
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "[WARN] Weights file not found: " << file_path << std::endl;
        return std::nullopt;
    }
    
    try {
        // Try Arrow CSV reader first
        std::shared_ptr<arrow::io::ReadableFile> infile;
        auto res = arrow::io::ReadableFile::Open(file_path);
        if (res.ok()) {
            infile = *res;
            
            // Read CSV with Arrow
            auto read_options = arrow::csv::ReadOptions::Defaults();
            auto parse_options = arrow::csv::ParseOptions::Defaults();
            auto convert_options = arrow::csv::ConvertOptions::Defaults();
            
            std::shared_ptr<arrow::csv::TableReader> reader;
            auto reader_res = arrow::csv::TableReader::Make(
                arrow::io::IOContext(arrow::default_memory_pool()), infile, read_options, parse_options, convert_options);
            
            if (reader_res.ok()) {
                reader = *reader_res;
                auto read_res = reader->Read();
                if (read_res.ok() && *read_res) {
                    std::shared_ptr<arrow::Table> table = *read_res;
                    auto weight_col = table->GetColumnByName("weight");
                    if (!weight_col || weight_col->type()->id() != arrow::Type::DOUBLE) {
                        // Weight column not double -> fallback to manual CSV parsing
                        table.reset();
                    }
                    if (table) {
                        auto symbol_col = table->GetColumnByName("symbol");
                        if (symbol_col && weight_col) {
                            for (int chunk = 0; chunk < symbol_col->num_chunks(); ++chunk) {
                                auto sym_arr = std::static_pointer_cast<arrow::StringArray>(symbol_col->chunk(chunk));
                                auto w_arr = std::static_pointer_cast<arrow::DoubleArray>(weight_col->chunk(chunk));
                                
                                for (int64_t i = 0; i < sym_arr->length(); ++i) {
                                    if (sym_arr->IsValid(i) && w_arr->IsValid(i)) {
                                        std::string symbol = sym_arr->GetString(i);
                                        double weight = w_arr->Value(i);
                                        if (std::isfinite(weight) && std::abs(weight) <= 10.0) {
                                            weights[symbol] = weight;
                                        }
                                    }
                                }
                            }
                            return weights;
                        }
                    }
                }
            }
        }
        
        // If weights map still empty after Arrow attempt, fallback to simple parsing
        if (weights.empty()) {
            // Fallback simple CSV parsing
            weights.clear();
            std::ifstream ifs(file_path);
            if (!ifs.is_open()) {
                std::cerr << "[WARN] Could not open weights file: " << file_path << std::endl;
                return std::nullopt;
            }
            
            std::string line;
            // Skip header
            if (!std::getline(ifs, line)) {
                std::cerr << "[WARN] Empty weights file: " << file_path << std::endl;
                return std::nullopt;
            }
            
            std::cerr << "[DEBUG] Using fallback CSV parsing for: " << file_path << std::endl;
            
            while (std::getline(ifs, line)) {
                if (line.empty()) continue;
                
                std::stringstream ss(line);
                std::string symbol, weight_str;
                
                if (std::getline(ss, symbol, ',') && std::getline(ss, weight_str, ',')) {
                    try {
                        double weight = std::stod(weight_str);
                        if (std::isfinite(weight) && std::abs(weight) <= 10.0) {
                            weights[symbol] = weight;
                            std::cerr << "[DEBUG] Loaded weight: " << symbol << " = " << weight << std::endl;
                        } else {
                            std::cerr << "[DEBUG] Skipped invalid weight: " << symbol << " = " << weight_str << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[WARN] Invalid weight value in " << file_path 
                                  << " for symbol " << symbol << ": " << e.what() << std::endl;
                    }
                }
            }
            
            std::cerr << "[DEBUG] Final weights map size: " << weights.size() << std::endl;
            for (const auto& pair : weights) {
                std::cerr << "[DEBUG] Final weight: " << pair.first << " = " << pair.second << std::endl;
            }
            
            return weights;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to load weights from " << file_path 
                  << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<std::unordered_map<std::string, double>> 
WeightsLoader::load_daily_weights(const std::string& base_path, 
                                 const std::chrono::system_clock::time_point& date) {
    std::string file_path = generate_filename(base_path, date);
    return load_weights_from_file(file_path);
}

} // namespace qse 