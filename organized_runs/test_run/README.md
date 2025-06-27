# QSE Trading Run Results

This directory contains the organized results from a Quantitative Strategy Engine (QSE) trading run.

## Directory Structure

### 📊 Data
- **`data/equity_curves/`**: Portfolio value over time for each strategy
- **`data/trade_logs/`**: Detailed trade execution logs  
- **`data/raw_data/`**: Original input data files

### 📈 Plots
- **`plots/strategy_comparisons/`**: Performance comparisons across strategies
- **`plots/performance_heatmaps/`**: Heatmaps showing performance metrics
- **`plots/individual_strategies/`**: Individual strategy performance plots

### 📋 Analysis
- **`analysis/reports/`**: Detailed analysis reports
- **`analysis/summaries/`**: Summary statistics and metrics

### 📝 Logs & Config
- **`logs/`**: Execution logs and error reports
- **`config/`**: Configuration files used for this run

## Quick Start

1. **View Summary**: Check `analysis/summaries/run_summary.txt` for an overview
2. **Analyze Performance**: Look at equity curves in `data/equity_curves/`
3. **Review Trades**: Examine trade logs in `data/trade_logs/`
4. **Visualize Results**: Browse plots in the `plots/` subdirectories

## File Naming Convention

- **Equity files**: `equity_[SYMBOL]_[STRATEGY]_[TIMESTAMP].csv`
- **Trade logs**: `tradelog_[SYMBOL]_[STRATEGY]_[TIMESTAMP].csv`
- **Plots**: Strategy-specific naming with timestamps

## Next Steps

- Run analysis scripts on the organized data
- Compare performance across different runs
- Archive this directory for future reference
