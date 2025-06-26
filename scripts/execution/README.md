# Execution Scripts

Scripts for running backtests and managing execution.

## Scripts

### `run_multi_symbol.sh`
**Multi-symbol backtesting**
- Runs backtests across multiple symbols
- Executes multiple strategies in parallel
- Generates comprehensive results

### `dev_mode.sh`
**Development mode execution**
- Runs with debug output enabled
- Uses smaller datasets for faster iteration
- Includes additional logging and validation

### `estimate_runtime.py`
**Runtime estimation**
- Estimates execution time for backtests
- Helps with resource planning
- Provides progress tracking

## Usage

```bash
# Run multi-symbol backtest
./scripts/run.sh run multi

# Development mode
./scripts/run.sh run dev
```

## Configuration

### Environment Variables
```bash
# Set initial capital
export QSE_INITIAL_CAPITAL=100000

# Set bar interval (seconds)
export QSE_BAR_INTERVAL=60

# Enable verbose output
export QSE_VERBOSE=1
```

### Common Parameters
- **Symbols**: AAPL, GOOG, MSFT, SPY
- **Strategies**: SMA_Crossover, FillTracking, PairsTrading
- **Time Period**: Configurable via data files
- **Bar Interval**: 60 seconds (1 minute)

## Output

Execution scripts generate:
- Equity curves (CSV format)
- Trade logs (CSV format)
- Performance metrics
- Timestamped result directories

## Monitoring

### Progress Tracking
```bash
# Monitor execution
tail -f engine.log

# Check results
ls -la results/

# View latest plots
ls -la plots/
```

## Dependencies

- QSE engine binaries
- Data files in `data/` directory
- Python analysis scripts 