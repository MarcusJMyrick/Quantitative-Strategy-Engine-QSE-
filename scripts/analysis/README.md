# Analysis Scripts

Scripts for analyzing backtest results and generating visualizations.

## Scripts

### `analyze.py`
**Single strategy analysis**
- Analyzes results from a single strategy run
- Generates equity curves and performance metrics
- Basic visualization and reporting

### `analyze_multi_strategy.py`
**Multi-strategy comparison**
- Compares multiple strategies across symbols
- Generates heatmaps and comparison plots
- Performance ranking and analysis

### `analyze_multi_run.py`
**Multiple run analysis**
- Analyzes results from multiple backtest runs
- Statistical analysis across runs
- Robustness testing

### `plot_current_run.py`
**Real-time plotting**
- Plots results from the most recent backtest
- Live visualization during execution
- Quick result preview

### `find_pairs.py`
**Pairs trading analysis**
- Identifies potential pairs for statistical arbitrage
- Correlation and cointegration analysis
- Pairs trading strategy optimization

## Usage

```bash
# Analyze single strategy
./scripts/run.sh analyze single

# Multi-strategy comparison
./scripts/run.sh analyze multi

# Plot current run
./scripts/run.sh analyze plot
```

## Output

All analysis scripts generate:
- Performance metrics (Sharpe ratio, drawdown, etc.)
- Visualization plots (PNG format)
- CSV reports with detailed statistics

## Dependencies

- pandas
- matplotlib
- seaborn
- numpy 