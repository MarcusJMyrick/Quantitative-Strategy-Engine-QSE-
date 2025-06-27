#include "qse/strategy/MultiFactorStrategy.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <fstream>

namespace qse {

    MultiFactorStrategy::MultiFactorStrategy(
        const std::vector<std::string>& symbols,
        int momentum_window,
        int momentum_short_window,
        int volatility_window,
        int value_window,
        int rebalance_frequency,
        int top_n,
        int bottom_n,
        std::shared_ptr<IOrderManager> order_manager)
        : symbols_(symbols)
        , momentum_window_(momentum_window)
        , momentum_short_window_(momentum_short_window)
        , volatility_window_(volatility_window)
        , value_window_(value_window)
        , rebalance_frequency_(rebalance_frequency)
        , top_n_(top_n)
        , bottom_n_(bottom_n)
        , days_since_rebalance_(0)
        , order_manager_(order_manager) {
        
        // Initialize moving average utilities for each symbol
        for (const auto& symbol : symbols_) {
            long_momentum_ma_[symbol] = std::make_unique<MovingAverage>(momentum_window);
            short_momentum_ma_[symbol] = std::make_unique<MovingAverage>(momentum_short_window);
            volatility_std_[symbol] = std::make_unique<MovingStandardDeviation>(volatility_window);
            value_ma_[symbol] = std::make_unique<MovingAverage>(value_window);
        }
    }

    void MultiFactorStrategy::on_tick(const Tick& tick) {
        // Multi-factor strategy operates on bars, not ticks
        // Ignore all ticks
    }

    void MultiFactorStrategy::on_bar(const Bar& bar) {
        // Only process bars for symbols we're tracking
        if (std::find(symbols_.begin(), symbols_.end(), bar.symbol) == symbols_.end()) {
            return;
        }

        // Store the latest bar
        latest_bars_[bar.symbol] = bar;
        
        // Update price history
        update_price_history(bar.symbol, bar.close);
        
        // Check if we have enough data for all symbols
        bool all_symbols_ready = true;
        for (const auto& symbol : symbols_) {
            if (price_history_[symbol].size() < std::max({momentum_window_, volatility_window_, value_window_})) {
                all_symbols_ready = false;
                break;
            }
        }
        
        if (!all_symbols_ready) {
            return; // Wait for more data
        }

        // Compute factors for all symbols
        for (const auto& symbol : symbols_) {
            double momentum = compute_momentum_factor(symbol);
            double volatility = compute_volatility_factor(symbol);
            double value = compute_value_factor(symbol);
            
            current_factors_[symbol] = FactorData(symbol, bar.timestamp, momentum, volatility, value, 0.0);
        }

        // Normalize factors and compute composite scores
        normalize_factors();
        compute_composite_scores();

        // Perform cross-sectional regression
        FactorReturns factor_returns = perform_cross_sectional_regression();
        factor_return_history_.push_back(factor_returns);

        // Check if it's time to rebalance
        if (should_rebalance()) {
            std::vector<PortfolioWeights> target_weights = construct_portfolio();
            execute_portfolio_changes(target_weights);
            days_since_rebalance_ = 0;
        } else {
            days_since_rebalance_++;
        }
    }

    void MultiFactorStrategy::on_fill(const Fill& fill) {
        // Update current weights based on fills
        double weight_change = (fill.side == "BUY" ? 1.0 : -1.0) * fill.quantity;
        current_weights_[fill.symbol] += weight_change;
    }

    double MultiFactorStrategy::compute_momentum_factor(const std::string& symbol) {
        if (price_history_[symbol].size() < momentum_window_) {
            return 0.0;
        }

        // Calculate long-term momentum (12-month returns)
        double long_return = 0.0;
        if (price_history_[symbol].size() >= momentum_window_) {
            double old_price = price_history_[symbol][price_history_[symbol].size() - momentum_window_];
            double current_price = price_history_[symbol].back();
            long_return = (current_price - old_price) / old_price;
        }

        // Calculate short-term momentum (1-month returns)
        double short_return = 0.0;
        if (price_history_[symbol].size() >= momentum_short_window_) {
            double old_price = price_history_[symbol][price_history_[symbol].size() - momentum_short_window_];
            double current_price = price_history_[symbol].back();
            short_return = (current_price - old_price) / old_price;
        }

        // Momentum factor: long-term minus short-term returns
        return long_return - short_return;
    }

    double MultiFactorStrategy::compute_volatility_factor(const std::string& symbol) {
        if (return_history_[symbol].size() < volatility_window_) {
            return 0.0;
        }

        // Calculate rolling volatility using standard deviation of returns
        std::vector<double> recent_returns;
        int start_idx = std::max(0, static_cast<int>(return_history_[symbol].size()) - volatility_window_);
        
        for (size_t i = start_idx; i < return_history_[symbol].size(); ++i) {
            recent_returns.push_back(return_history_[symbol][i]);
        }

        if (recent_returns.empty()) {
            return 0.0;
        }

        // Calculate mean return
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / recent_returns.size();

        // Calculate variance
        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= recent_returns.size();

        // Return annualized volatility (assuming daily data)
        return std::sqrt(variance * 252.0);
    }

    double MultiFactorStrategy::compute_value_factor(const std::string& symbol) {
        if (price_history_[symbol].size() < value_window_) {
            return 0.0;
        }

        // Simple value proxy: negative of recent returns (mean reversion)
        // In a real implementation, this would use P/E, P/B ratios, etc.
        double recent_return = 0.0;
        if (price_history_[symbol].size() >= value_window_) {
            double old_price = price_history_[symbol][price_history_[symbol].size() - value_window_];
            double current_price = price_history_[symbol].back();
            recent_return = (current_price - old_price) / old_price;
        }

        // Value factor: negative of recent returns (cheap stocks have low recent returns)
        return -recent_return;
    }

    void MultiFactorStrategy::normalize_factors() {
        // Calculate cross-sectional means and standard deviations
        std::vector<double> momentum_values, volatility_values, value_values;
        
        for (const auto& symbol : symbols_) {
            if (current_factors_.find(symbol) != current_factors_.end()) {
                momentum_values.push_back(current_factors_[symbol].momentum);
                volatility_values.push_back(current_factors_[symbol].volatility);
                value_values.push_back(current_factors_[symbol].value);
            }
        }

        // Calculate statistics for each factor
        auto calculate_stats = [](const std::vector<double>& values) -> std::pair<double, double> {
            if (values.empty()) return {0.0, 1.0};
            
            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double variance = 0.0;
            for (double val : values) {
                variance += (val - mean) * (val - mean);
            }
            variance /= values.size();
            double std_dev = std::sqrt(variance);
            
            return {mean, std_dev > 0.0 ? std_dev : 1.0};
        };

        auto momentum_stats = calculate_stats(momentum_values);
        auto volatility_stats = calculate_stats(volatility_values);
        auto value_stats = calculate_stats(value_values);

        // Normalize factors to z-scores
        for (auto& pair : current_factors_) {
            FactorData& factor = pair.second;
            
            factor.momentum = (factor.momentum - momentum_stats.first) / momentum_stats.second;
            factor.volatility = (factor.volatility - volatility_stats.first) / volatility_stats.second;
            factor.value = (factor.value - value_stats.first) / value_stats.second;
        }
    }

    void MultiFactorStrategy::compute_composite_scores() {
        // Simple equal-weighted composite score
        // In practice, you might use factor returns to weight the factors
        for (auto& pair : current_factors_) {
            FactorData& factor = pair.second;
            factor.composite_score = (factor.momentum + factor.volatility + factor.value) / 3.0;
        }
    }

    FactorReturns MultiFactorStrategy::perform_cross_sectional_regression() {
        // Simple cross-sectional regression to estimate factor returns
        // In practice, you'd use a more sophisticated regression library
        
        std::vector<double> returns, momentum_exposures, volatility_exposures, value_exposures;
        
        for (const auto& symbol : symbols_) {
            if (current_factors_.find(symbol) != current_factors_.end() && 
                return_history_[symbol].size() > 0) {
                
                // Use the most recent return
                returns.push_back(return_history_[symbol].back());
                momentum_exposures.push_back(current_factors_[symbol].momentum);
                volatility_exposures.push_back(current_factors_[symbol].volatility);
                value_exposures.push_back(current_factors_[symbol].value);
            }
        }

        if (returns.size() < 4) {
            // Not enough data for regression
            return FactorReturns(Timestamp(), 0.0, 0.0, 0.0, 0.0);
        }

        // Simple OLS regression using normal equations
        // For simplicity, we'll use a basic implementation
        // In practice, you'd use a proper linear algebra library
        
        // Calculate means
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double mean_momentum = std::accumulate(momentum_exposures.begin(), momentum_exposures.end(), 0.0) / momentum_exposures.size();
        double mean_volatility = std::accumulate(volatility_exposures.begin(), volatility_exposures.end(), 0.0) / volatility_exposures.size();
        double mean_value = std::accumulate(value_exposures.begin(), value_exposures.end(), 0.0) / value_exposures.size();

        // Calculate covariances and variances
        double cov_momentum = 0.0, cov_volatility = 0.0, cov_value = 0.0;
        double var_momentum = 0.0, var_volatility = 0.0, var_value = 0.0;
        
        for (size_t i = 0; i < returns.size(); ++i) {
            double ret_dev = returns[i] - mean_return;
            double mom_dev = momentum_exposures[i] - mean_momentum;
            double vol_dev = volatility_exposures[i] - mean_volatility;
            double val_dev = value_exposures[i] - mean_value;
            
            cov_momentum += ret_dev * mom_dev;
            cov_volatility += ret_dev * vol_dev;
            cov_value += ret_dev * val_dev;
            
            var_momentum += mom_dev * mom_dev;
            var_volatility += vol_dev * vol_dev;
            var_value += val_dev * val_dev;
        }

        // Calculate factor returns (betas)
        double momentum_return = var_momentum > 0.0 ? cov_momentum / var_momentum : 0.0;
        double volatility_return = var_volatility > 0.0 ? cov_volatility / var_volatility : 0.0;
        double value_return = var_value > 0.0 ? cov_value / var_value : 0.0;

        return FactorReturns(Timestamp(), momentum_return, volatility_return, value_return, mean_return);
    }

    std::vector<PortfolioWeights> MultiFactorStrategy::construct_portfolio() {
        std::vector<PortfolioWeights> weights;
        
        // Sort symbols by composite score
        std::vector<std::pair<std::string, double>> sorted_symbols;
        for (const auto& pair : current_factors_) {
            sorted_symbols.push_back({pair.first, pair.second.composite_score});
        }
        
        std::sort(sorted_symbols.begin(), sorted_symbols.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Assign weights: long top N, short bottom N
        for (size_t i = 0; i < sorted_symbols.size(); ++i) {
            const std::string& symbol = sorted_symbols[i].first;
            double weight = 0.0;
            
            if (i < static_cast<size_t>(top_n_)) {
                weight = 1.0 / top_n_; // Equal weight long
            } else if (i >= sorted_symbols.size() - static_cast<size_t>(bottom_n_)) {
                weight = -1.0 / bottom_n_; // Equal weight short
            }
            
            weights.push_back(PortfolioWeights(symbol, weight, weight));
        }

        // Apply constraints
        apply_sector_neutrality(weights);
        apply_turnover_penalty(weights);

        return weights;
    }

    void MultiFactorStrategy::apply_sector_neutrality(std::vector<PortfolioWeights>& weights) {
        // For now, we'll implement a simple sector neutrality
        // In practice, you'd have sector classifications for each stock
        
        // Calculate sector weights (assuming equal sector distribution for simplicity)
        double total_long_weight = 0.0;
        double total_short_weight = 0.0;
        
        for (const auto& weight : weights) {
            if (weight.weight > 0) {
                total_long_weight += weight.weight;
            } else {
                total_short_weight += std::abs(weight.weight);
            }
        }

        // Normalize to ensure sector neutrality
        if (total_long_weight > 0 && total_short_weight > 0) {
            double scale_factor = std::min(total_long_weight, total_short_weight) / 
                                 std::max(total_long_weight, total_short_weight);
            
            for (auto& weight : weights) {
                if (weight.weight > 0) {
                    weight.sector_neutral_weight = weight.weight * scale_factor;
                } else {
                    weight.sector_neutral_weight = weight.weight / scale_factor;
                }
            }
        }
    }

    void MultiFactorStrategy::apply_turnover_penalty(std::vector<PortfolioWeights>& weights) {
        // Simple turnover penalty: reduce weight changes
        const double turnover_penalty = 0.1; // 10% penalty on weight changes
        
        for (auto& weight : weights) {
            double current_weight = current_weights_[weight.symbol];
            double weight_change = weight.sector_neutral_weight - current_weight;
            
            // Apply penalty to weight changes
            weight.sector_neutral_weight = current_weight + weight_change * (1.0 - turnover_penalty);
        }
    }

    double MultiFactorStrategy::calculate_return(const std::string& symbol) {
        if (price_history_[symbol].size() < 2) {
            return 0.0;
        }
        
        double current_price = price_history_[symbol].back();
        double previous_price = price_history_[symbol][price_history_[symbol].size() - 2];
        
        return (current_price - previous_price) / previous_price;
    }

    void MultiFactorStrategy::update_price_history(const std::string& symbol, double price) {
        price_history_[symbol].push_back(price);
        
        // Keep only the necessary history
        size_t max_history = std::max({static_cast<size_t>(momentum_window_), 
                                      static_cast<size_t>(volatility_window_), 
                                      static_cast<size_t>(value_window_)}) + 10;
        
        if (price_history_[symbol].size() > max_history) {
            price_history_[symbol].pop_front();
        }
        
        // Calculate and store return
        if (price_history_[symbol].size() >= 2) {
            double return_val = calculate_return(symbol);
            return_history_[symbol].push_back(return_val);
            
            // Keep return history in sync
            if (return_history_[symbol].size() > max_history) {
                return_history_[symbol].pop_front();
            }
        }
    }

    bool MultiFactorStrategy::should_rebalance() {
        return days_since_rebalance_ >= rebalance_frequency_;
    }

    void MultiFactorStrategy::execute_portfolio_changes(const std::vector<PortfolioWeights>& target_weights) {
        if (!order_manager_) {
            return;
        }

        for (const auto& target_weight : target_weights) {
            double current_weight = current_weights_[target_weight.symbol];
            double weight_diff = target_weight.sector_neutral_weight - current_weight;
            
            if (std::abs(weight_diff) > 0.001) { // Minimum threshold
                // Calculate quantity to trade
                // This is a simplified implementation - in practice you'd use position sizing
                int quantity = static_cast<int>(weight_diff * 1000); // Assume $1000 per unit weight
                
                if (quantity > 0) {
                    // Buy order
                    Order order;
                    order.symbol = target_weight.symbol;
                    order.side = Order::Side::BUY;
                    order.type = Order::Type::MARKET;
                    order.quantity = quantity;
                    order.time_in_force = Order::TimeInForce::DAY;
                    order.timestamp = Timestamp();
                    
                    // Note: submit_order method doesn't exist in IOrderManager interface
                    // This would need to be implemented in the concrete OrderManager class
                    // order_manager_->submit_order(order);
                } else if (quantity < 0) {
                    // Sell order
                    Order order;
                    order.symbol = target_weight.symbol;
                    order.side = Order::Side::SELL;
                    order.type = Order::Type::MARKET;
                    order.quantity = -quantity;
                    order.time_in_force = Order::TimeInForce::DAY;
                    order.timestamp = Timestamp();
                    
                    // Note: submit_order method doesn't exist in IOrderManager interface
                    // This would need to be implemented in the concrete OrderManager class
                    // order_manager_->submit_order(order);
                }
            }
        }
    }

} // namespace qse 