#include "qse/factor/AlphaBlender.h"
#include <arrow/array.h>
#include <arrow/array/array_primitive.h>
#include <arrow/array/array_binary.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/builder.h>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace qse {

bool AlphaBlender::load_config(const std::string& config_path) {
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        
        // Load factor weights
        if (config["factor_weights"]) {
            config_.factor_weights.clear();
            for (const auto& factor : config["factor_weights"]) {
                std::string factor_name = factor.first.as<std::string>();
                double weight = factor.second.as<double>();
                config_.factor_weights[factor_name] = weight;
            }
        }
        
        // Load IR weighting settings
        if (config["use_ir_weighting"]) {
            config_.use_ir_weighting = config["use_ir_weighting"].as<bool>();
        }
        
        if (config["ir_lookback_period"]) {
            config_.ir_lookback_period = config["ir_lookback_period"].as<double>();
        }
        
        if (config["min_ir_weight"]) {
            config_.min_ir_weight = config["min_ir_weight"].as<double>();
        }
        
        if (config["max_ir_weight"]) {
            config_.max_ir_weight = config["max_ir_weight"].as<double>();
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading AlphaBlender config: " << e.what() << std::endl;
        return false;
    }
}

void AlphaBlender::set_config(const BlendingConfig& config) {
    config_ = config;
}

AlphaBlender::BlendingResult AlphaBlender::blend_factors(
    const std::shared_ptr<arrow::Table>& table,
    const std::vector<std::string>& factor_cols,
    const std::string& return_col,
    const std::string& date_col) {
    
    BlendingResult result;
    std::map<std::string, double> final_weights;
    std::map<std::string, double> factor_irs;
    
    if (config_.use_ir_weighting) {
        // Calculate IR-weighted weights
        final_weights = calculate_ir_weights(table, factor_cols, return_col, date_col);
        
        // Calculate IR values for each factor
        for (const auto& factor_col : factor_cols) {
            auto factor_array = std::static_pointer_cast<arrow::DoubleArray>(
                table->GetColumnByName(factor_col)->chunk(0));
            auto return_array = std::static_pointer_cast<arrow::DoubleArray>(
                table->GetColumnByName(return_col)->chunk(0));
            
            std::vector<double> factor_values(factor_array->length());
            std::vector<double> return_values(return_array->length());
            
            for (int64_t i = 0; i < factor_array->length(); ++i) {
                factor_values[i] = factor_array->Value(i);
                return_values[i] = return_array->Value(i);
            }
            
            factor_irs[factor_col] = calculate_ir(factor_values, return_values);
        }
    } else {
        // Use YAML-configured weights
        final_weights = config_.factor_weights;
        
        // Normalize weights if they don't sum to 1.0
        double total_weight = 0.0;
        for (const auto& weight : final_weights) {
            total_weight += weight.second;
        }
        
        if (std::abs(total_weight - 1.0) > 1e-6) {
            final_weights = normalize_weights(final_weights);
        }
    }
    
    // Apply weights to create alpha score
    result.table = apply_weights(table, factor_cols, final_weights);
    result.final_weights = final_weights;
    result.factor_irs = factor_irs;
    
    return result;
}

double AlphaBlender::calculate_ir(const std::vector<double>& factor_values,
                                 const std::vector<double>& return_values) {
    if (factor_values.size() != return_values.size() || factor_values.empty()) {
        return 0.0;
    }
    
    // Calculate correlation between factor and returns
    double sum_f = 0.0, sum_r = 0.0, sum_fr = 0.0, sum_f2 = 0.0, sum_r2 = 0.0;
    int n = factor_values.size();
    
    for (int i = 0; i < n; ++i) {
        sum_f += factor_values[i];
        sum_r += return_values[i];
        sum_fr += factor_values[i] * return_values[i];
        sum_f2 += factor_values[i] * factor_values[i];
        sum_r2 += return_values[i] * return_values[i];
    }
    
    double mean_f = sum_f / n;
    double mean_r = sum_r / n;
    
    double numerator = sum_fr - n * mean_f * mean_r;
    double denominator = std::sqrt((sum_f2 - n * mean_f * mean_f) * 
                                  (sum_r2 - n * mean_r * mean_r));
    
    if (std::abs(denominator) < 1e-10) {
        return 0.0;
    }
    
    double correlation = numerator / denominator;
    
    // Information Ratio is approximately the correlation
    // In practice, IR = IC / IC_std, but for simplicity we use correlation
    return correlation;
}

std::map<std::string, double> AlphaBlender::calculate_ir_weights(
    const std::shared_ptr<arrow::Table>& table,
    const std::vector<std::string>& factor_cols,
    const std::string& return_col,
    const std::string& date_col) {
    
    std::map<std::string, double> ir_weights;
    std::map<std::string, double> factor_irs;
    
    // Calculate IR for each factor
    for (const auto& factor_col : factor_cols) {
        auto factor_array = std::static_pointer_cast<arrow::DoubleArray>(
            table->GetColumnByName(factor_col)->chunk(0));
        auto return_array = std::static_pointer_cast<arrow::DoubleArray>(
            table->GetColumnByName(return_col)->chunk(0));
        
        std::vector<double> factor_values(factor_array->length());
        std::vector<double> return_values(return_array->length());
        
        for (int64_t i = 0; i < factor_array->length(); ++i) {
            factor_values[i] = factor_array->Value(i);
            return_values[i] = return_array->Value(i);
        }
        
        double ir = calculate_ir(factor_values, return_values);
        factor_irs[factor_col] = ir;
    }
    
    // Convert IRs to weights with bounds
    for (const auto& factor_ir : factor_irs) {
        double ir_abs = std::abs(factor_ir.second);
        double weight = std::max(config_.min_ir_weight, 
                                std::min(config_.max_ir_weight, ir_abs));
        ir_weights[factor_ir.first] = weight;
    }
    
    // Normalize weights
    return normalize_weights(ir_weights);
}

std::map<std::string, double> AlphaBlender::normalize_weights(
    const std::map<std::string, double>& weights) {
    
    std::map<std::string, double> normalized;
    double total_weight = 0.0;
    
    for (const auto& weight : weights) {
        total_weight += weight.second;
    }
    
    if (total_weight > 0) {
        for (const auto& weight : weights) {
            normalized[weight.first] = weight.second / total_weight;
        }
    }
    
    return normalized;
}

std::shared_ptr<arrow::Table> AlphaBlender::apply_weights(
    const std::shared_ptr<arrow::Table>& table,
    const std::vector<std::string>& factor_cols,
    const std::map<std::string, double>& weights) {
    
    // Get the number of rows
    int64_t num_rows = table->num_rows();
    
    // Create alpha score array
    std::vector<double> alpha_scores(num_rows, 0.0);
    
    // Apply weights to each factor
    for (const auto& factor_col : factor_cols) {
        auto weight_it = weights.find(factor_col);
        if (weight_it == weights.end()) {
            continue; // Skip factors without weights
        }
        
        double weight = weight_it->second;
        auto factor_array = std::static_pointer_cast<arrow::DoubleArray>(
            table->GetColumnByName(factor_col)->chunk(0));
        
        for (int64_t i = 0; i < num_rows; ++i) {
            alpha_scores[i] += weight * factor_array->Value(i);
        }
    }
    
    // Create Arrow array for alpha scores
    arrow::DoubleBuilder builder;
    auto status = builder.AppendValues(alpha_scores);
    if (!status.ok()) {
        std::cerr << "Error appending alpha scores: " << status.ToString() << std::endl;
        return nullptr;
    }
    
    std::shared_ptr<arrow::Array> alpha_array;
    status = builder.Finish(&alpha_array);
    if (!status.ok()) {
        std::cerr << "Error finishing alpha array: " << status.ToString() << std::endl;
        return nullptr;
    }
    
    // Create field for alpha score column
    auto alpha_field = arrow::field("alpha_score", arrow::float64());
    
    // Add alpha score column to table
    std::vector<std::shared_ptr<arrow::Field>> fields = table->schema()->fields();
    fields.push_back(alpha_field);
    
    std::vector<std::shared_ptr<arrow::ChunkedArray>> chunked_arrays = table->columns();
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Convert chunked arrays to single arrays
    for (const auto& chunked_array : chunked_arrays) {
        if (chunked_array->num_chunks() > 0) {
            arrays.push_back(chunked_array->chunk(0));
        }
    }
    arrays.push_back(alpha_array);
    
    auto new_schema = arrow::schema(fields);
    auto result = arrow::Table::Make(new_schema, arrays);
    return result;
}

} // namespace qse 