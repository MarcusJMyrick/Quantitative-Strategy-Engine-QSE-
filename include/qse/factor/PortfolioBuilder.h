#pragma once
#include <string>
#include <memory>
#include <vector>
#include <Eigen/Dense>
#include <yaml-cpp/yaml.h>

namespace arrow {
    class Table;
}

namespace qse {

/**
 * @class PortfolioBuilder
 * @brief Portfolio optimizer that maximizes alpha while honoring constraints
 * 
 * This class implements a quadratic programming optimizer that:
 * 1. Maximizes alpha score: Σ αᵢwᵢ
 * 2. Minimizes risk: -γ Σ wᵢ² (L2 regularization)
 * 3. Honors constraints:
 *    - Net exposure = 0 (Σ wᵢ = 0)
 *    - Gross exposure ≤ 2× (|long| + |short|)
 *    - Beta neutral (Σ βᵢwᵢ ≈ 0)
 *    - Optional sector caps and turnover limits
 */
class PortfolioBuilder {
public:
    struct OptimizationConfig {
        double gamma = 0.01;           // Risk aversion parameter
        double gross_cap = 2.0;        // Maximum gross exposure
        double beta_target = 0.0;      // Target portfolio beta
        double beta_tolerance = 1e-6;  // Beta constraint tolerance
        int max_iterations = 1000;     // Maximum optimization iterations
        double convergence_tol = 1e-6; // Convergence tolerance
    };

    struct OptimizationResult {
        std::vector<double> weights;   // Optimal weights
        double objective_value;        // Final objective value
        double net_exposure;           // Σ wᵢ
        double gross_exposure;         // Σ |wᵢ|
        double portfolio_beta;         // Σ βᵢwᵢ
        int iterations;                // Number of iterations
        bool converged;                // Whether optimization converged
    };

    PortfolioBuilder() = default;
    ~PortfolioBuilder() = default;

    /**
     * @brief Set optimization configuration
     * @param config Optimization parameters
     */
    void set_config(const OptimizationConfig& config);

    /**
     * @brief Load configuration from YAML file
     * @param yaml_path Path to YAML configuration file
     */
    void load_config(const std::string& yaml_path);

    /**
     * @brief Optimize portfolio weights
     * @param alpha_scores Vector of alpha scores for each asset
     * @param betas Vector of beta values for each asset
     * @param symbols Vector of asset symbols (for output)
     * @return Optimization result with weights and metrics
     */
    OptimizationResult optimize(const std::vector<double>& alpha_scores,
                               const std::vector<double>& betas,
                               const std::vector<std::string>& symbols);

    /**
     * @brief Optimize from Arrow table with factor data
     * @param factor_table Table containing alpha scores and betas
     * @param alpha_col Column name for alpha scores
     * @param beta_col Column name for beta values
     * @param symbol_col Column name for asset symbols
     * @return Optimization result
     */
    OptimizationResult optimize_from_table(const std::shared_ptr<arrow::Table>& factor_table,
                                          const std::string& alpha_col,
                                          const std::string& beta_col,
                                          const std::string& symbol_col);

    /**
     * @brief Save weights to CSV file
     * @param result Optimization result
     * @param symbols Asset symbols
     * @param output_path Output file path
     */
    void save_weights(const OptimizationResult& result,
                     const std::vector<std::string>& symbols,
                     const std::string& output_path);

private:
    // Core optimization using projected gradient descent
    OptimizationResult solve_qp(const Eigen::VectorXd& alpha,
                               const Eigen::VectorXd& beta);

    // Constraint checking functions
    double compute_net_exposure(const Eigen::VectorXd& weights);
    double compute_gross_exposure(const Eigen::VectorXd& weights);
    double compute_portfolio_beta(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta);
    bool check_constraints(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta);

    // Projection onto constraint set
    Eigen::VectorXd project_to_constraints(const Eigen::VectorXd& weights, const Eigen::VectorXd& beta);

    // Configuration
    OptimizationConfig config_;
};

} // namespace qse 