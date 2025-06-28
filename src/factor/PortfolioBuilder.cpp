#include "qse/factor/PortfolioBuilder.h"
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/builder.h>
#include <arrow/csv/writer.h>
#include <arrow/ipc/writer.h>
#include <arrow/io/api.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace qse;

void PortfolioBuilder::set_config(const OptimizationConfig& config) {
    config_ = config;
}

void PortfolioBuilder::load_config(const std::string& yaml_path) {
    YAML::Node config = YAML::LoadFile(yaml_path);
    
    if (config["portfolio_optimizer"]) {
        auto opt_config = config["portfolio_optimizer"];
        config_.gamma = opt_config["gamma"].as<double>();
        config_.gross_cap = opt_config["gross_cap"].as<double>();
        config_.beta_target = opt_config["beta_target"].as<double>();
        config_.beta_tolerance = opt_config["beta_tolerance"].as<double>();
        config_.max_iterations = opt_config["max_iterations"].as<int>();
        config_.convergence_tol = opt_config["convergence_tol"].as<double>();
    }
}

PortfolioBuilder::OptimizationResult PortfolioBuilder::optimize(
    const std::vector<double>& alpha_scores,
    const std::vector<double>& betas,
    const std::vector<std::string>& symbols) {
    
    if (alpha_scores.size() != betas.size() || alpha_scores.size() != symbols.size()) {
        throw std::invalid_argument("Input vectors must have the same size");
    }
    
    if (alpha_scores.empty()) {
        throw std::invalid_argument("Input vectors cannot be empty");
    }
    
    // Convert to Eigen vectors
    Eigen::VectorXd alpha = Eigen::Map<const Eigen::VectorXd>(alpha_scores.data(), alpha_scores.size());
    Eigen::VectorXd beta = Eigen::Map<const Eigen::VectorXd>(betas.data(), betas.size());
    
    // Solve the QP problem
    return solve_qp(alpha, beta);
}

PortfolioBuilder::OptimizationResult PortfolioBuilder::optimize_from_table(
    const std::shared_ptr<arrow::Table>& factor_table,
    const std::string& alpha_col,
    const std::string& beta_col,
    const std::string& symbol_col) {
    
    if (!factor_table) {
        throw std::invalid_argument("Factor table is null");
    }
    
    // Extract columns
    auto alpha_chunked = factor_table->GetColumnByName(alpha_col);
    auto beta_chunked = factor_table->GetColumnByName(beta_col);
    auto symbol_chunked = factor_table->GetColumnByName(symbol_col);
    
    if (!alpha_chunked || !beta_chunked || !symbol_chunked) {
        throw std::runtime_error("Required columns not found in factor table");
    }
    
    // For simplicity, assume single chunk arrays
    if (alpha_chunked->num_chunks() != 1 || beta_chunked->num_chunks() != 1 || symbol_chunked->num_chunks() != 1) {
        throw std::runtime_error("Multi-chunk arrays not supported");
    }
    
    auto alpha_array = std::static_pointer_cast<arrow::DoubleArray>(alpha_chunked->chunk(0));
    auto beta_array = std::static_pointer_cast<arrow::DoubleArray>(beta_chunked->chunk(0));
    auto symbol_array = std::static_pointer_cast<arrow::StringArray>(symbol_chunked->chunk(0));
    
    if (!alpha_array || !beta_array || !symbol_array) {
        throw std::runtime_error("Invalid array types");
    }
    
    // Extract data
    std::vector<double> alpha_scores, betas;
    std::vector<std::string> symbols;
    
    int n = alpha_array->length();
    for (int i = 0; i < n; ++i) {
        if (alpha_array->IsValid(i) && beta_array->IsValid(i) && symbol_array->IsValid(i)) {
            alpha_scores.push_back(alpha_array->Value(i));
            betas.push_back(beta_array->Value(i));
            
            auto sv = symbol_array->GetView(i);
            symbols.emplace_back(sv.data(), sv.size());
        }
    }
    
    return optimize(alpha_scores, betas, symbols);
}

PortfolioBuilder::OptimizationResult PortfolioBuilder::solve_qp(
    const Eigen::VectorXd& alpha,
    const Eigen::VectorXd& beta) {
    
    OptimizationResult result;
    int n = alpha.size();
    result.converged = false;
    
    // Initialize weights to zero
    Eigen::VectorXd weights = Eigen::VectorXd::Zero(n);
    
    // Projected gradient descent
    double step_size = 0.01;
    double prev_objective = std::numeric_limits<double>::lowest();
    
    for (int iter = 0; iter < config_.max_iterations; ++iter) {
        // Compute gradient: ∇f = α - 2γw
        Eigen::VectorXd gradient = alpha - 2.0 * config_.gamma * weights;
        
        // Update weights: w = w + step_size * gradient
        weights = weights + step_size * gradient;
        
        // Project onto constraint set
        weights = project_to_constraints(weights, beta);
        
        // Compute objective value: Σ αᵢwᵢ - γ Σ wᵢ²
        double objective = alpha.dot(weights) - config_.gamma * weights.squaredNorm();
        
        // Check convergence
        if (std::abs(objective - prev_objective) < config_.convergence_tol) {
            result.converged = true;
            result.iterations = iter + 1;
            break;
        }
        
        prev_objective = objective;
    }

    if (!result.converged) {
        result.iterations = config_.max_iterations;
    }
    
    // Store results
    result.weights.resize(n);
    Eigen::Map<Eigen::VectorXd>(result.weights.data(), n) = weights;
    
    result.objective_value = alpha.dot(weights) - config_.gamma * weights.squaredNorm();
    result.net_exposure = compute_net_exposure(weights);
    result.gross_exposure = compute_gross_exposure(weights);
    result.portfolio_beta = compute_portfolio_beta(weights, beta);
    
    return result;
}

double PortfolioBuilder::compute_net_exposure(const Eigen::VectorXd& weights) {
    return weights.sum();
}

double PortfolioBuilder::compute_gross_exposure(const Eigen::VectorXd& weights) {
    return weights.array().abs().sum();
}

double PortfolioBuilder::compute_portfolio_beta(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta) {
    return weights.dot(beta);
}

bool PortfolioBuilder::check_constraints(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta) {
    double net_exposure = compute_net_exposure(weights);
    double gross_exposure = compute_gross_exposure(weights);
    double portfolio_beta = compute_portfolio_beta(weights, beta);
    
    return std::abs(net_exposure) < config_.convergence_tol &&
           gross_exposure <= config_.gross_cap &&
           std::abs(portfolio_beta - config_.beta_target) < config_.beta_tolerance;
}

Eigen::VectorXd PortfolioBuilder::project_to_constraints(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta) {
    Eigen::VectorXd projected = weights;
    int n = weights.size();
    
    // Step 1: Project to net exposure = 0
    double net_exposure = compute_net_exposure(projected);
    if (std::abs(net_exposure) > 1e-9) { // Use a small epsilon
        projected.array() -= net_exposure / n;
    }
    
    // Step 2: Project to beta neutral
    double portfolio_beta = compute_portfolio_beta(projected, beta);
    if (std::abs(portfolio_beta - config_.beta_target) > config_.beta_tolerance) {
        double beta_norm_sq = beta.squaredNorm();
        if (beta_norm_sq > 1e-9) {
            Eigen::VectorXd beta_adjusted = beta.array() - beta.mean();
            double beta_dot_adjusted_beta = projected.dot(beta_adjusted);

            if (std::abs(beta_dot_adjusted_beta) > 1e-9)
            {
                double beta_adjustment = (portfolio_beta - config_.beta_target) / (beta.dot(beta_adjusted));
                projected -= beta_adjustment * beta_adjusted;
            }
        }
    }
     // Re-project to net exposure = 0 as beta projection might have shifted it
    net_exposure = compute_net_exposure(projected);
    if (std::abs(net_exposure) > 1e-9) {
        projected.array() -= net_exposure / n;
    }

    // Step 3: Project to gross exposure constraint
    double gross_exposure = compute_gross_exposure(projected);
    if (gross_exposure > config_.gross_cap) {
        projected *= (config_.gross_cap / gross_exposure);
    }
    
    return projected;
}

void PortfolioBuilder::save_weights(const OptimizationResult& result,
                                   const std::vector<std::string>& symbols,
                                   const std::string& output_path) {
    if (result.weights.size() != symbols.size()) {
        throw std::invalid_argument("Weights and symbols must have the same size");
    }
    
    // Create Arrow table for CSV output
    arrow::StringBuilder symbol_builder;
    arrow::DoubleBuilder weight_builder;
    
    for (size_t i = 0; i < symbols.size(); ++i) {
        symbol_builder.Append(symbols[i]);
        weight_builder.Append(result.weights[i]);
    }
    
    std::shared_ptr<arrow::Array> symbol_array, weight_array;
    auto status = symbol_builder.Finish(&symbol_array);
    if (!status.ok()) throw std::runtime_error(status.message());
    status = weight_builder.Finish(&weight_array);
    if (!status.ok()) throw std::runtime_error(status.message());
    
    auto schema = arrow::schema({
        arrow::field("symbol", arrow::utf8()),
        arrow::field("weight", arrow::float64())
    });
    
    auto table = arrow::Table::Make(schema, {symbol_array, weight_array});
    
    // Write to CSV
    auto output_file_result = arrow::io::FileOutputStream::Open(output_path);
    if (!output_file_result.ok()) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }
    std::shared_ptr<arrow::io::FileOutputStream> output_file = *output_file_result;
    
    auto writer_result = arrow::csv::MakeCSVWriter(output_file, table->schema());
     if (!writer_result.ok()) {
        throw std::runtime_error("Failed to create CSV writer");
    }
    std::shared_ptr<arrow::ipc::RecordBatchWriter> csv_writer = *writer_result;
    
    status = csv_writer->WriteTable(*table);
    if (!status.ok()) {
        throw std::runtime_error("Failed to write CSV: " + status.ToString());
    }
    
    status = csv_writer->Close();
    if (!status.ok()) {
        throw std::runtime_error("Failed to close CSV writer: " + status.ToString());
    }
    
    std::cout << "Saved portfolio weights to: " << output_path << std::endl;
}