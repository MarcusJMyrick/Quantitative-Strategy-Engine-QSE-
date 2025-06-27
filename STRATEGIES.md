# 📊 QSE Trading Strategies Guide

## 🎯 Available Trading Strategies

Your QSE system includes the following trading strategies:

### 1. **SMA Crossover Strategy** (`SMACrossoverStrategy`)
- **Purpose**: Trend-following strategy using Simple Moving Average crossovers
- **Logic**: 
  - Buy when short-term SMA (20) crosses above long-term SMA (50)
  - Sell when short-term SMA crosses below long-term SMA
- **Parameters**: 20-period and 50-period moving averages
- **Best for**: Trending markets, medium-term positions
- **Files**: `src/strategy/SMACrossoverStrategy.cpp`, `include/qse/strategy/SMACrossoverStrategy.h`

### 2. **Fill Tracking Strategy** (`FillTrackingStrategy`)
- **Purpose**: Testing strategy that generates guaranteed trades
- **Logic**: Submits a market order on the first tick to verify the trading system works
- **Parameters**: None (automatic)
- **Best for**: System testing, smoke tests, verifying order execution
- **Files**: `src/strategy/FillTrackingStrategy.cpp`, `include/qse/strategy/FillTrackingStrategy.h`

### 3. **Pairs Trading Strategy** (`PairsTradingStrategy`)
- **Purpose**: Statistical arbitrage between correlated assets
- **Logic**: 
  - Monitors price relationship between two symbols
  - Trades when the spread deviates from historical mean
  - Mean-reversion approach
- **Parameters**: Two symbols (e.g., AAPL vs GOOG)
- **Best for**: Market-neutral strategies, statistical arbitrage
- **Files**: `src/strategy/PairsTradingStrategy.cpp`, `include/qse/strategy/PairsTradingStrategy.h`

### 4. **Do Nothing Strategy** (`DoNothingStrategy`)
- **Purpose**: Baseline strategy that does nothing
- **Logic**: Receives all ticks but takes no action
- **Parameters**: None
- **Best for**: Performance baseline, testing framework
- **Files**: `include/qse/strategy/DoNothingStrategy.h`

## 🔧 Utility Classes (Not Trading Strategies)

These are mathematical utilities that can be used within strategies:

### **Moving Average** (`MovingAverage`)
- **Purpose**: Calculates simple moving average of price series
- **Use**: Can be used within strategies to calculate trend indicators
- **Files**: `src/strategy/MovingAverage.cpp`, `include/qse/strategy/MovingAverage.h`

### **Moving Standard Deviation** (`MovingStandardDeviation`)
- **Purpose**: Calculates rolling standard deviation of price series
- **Use**: Can be used for volatility-based strategies or risk management
- **Files**: `src/strategy/MovingStandardDeviation.cpp`, `include/qse/strategy/MovingStandardDeviation.h`

## 🚀 How to Run All Strategies

### Option 1: Run All Strategies (Recommended)
```bash
./scripts/run_all_strategies.sh
```

This will run:
- ✅ SMA Crossover for all symbols (AAPL, GOOG, MSFT, SPY)
- ✅ Fill Tracking for all symbols
- ✅ Do Nothing baseline for all symbols
- ✅ Pairs Trading (AAPL vs GOOG)

### Option 2: Run Individual Strategies
```bash
# Build the engine
./scripts/build/build.sh

# Run specific engine
./build/src/engine/multi_strategy_engine  # All strategies
./build/src/engine/multi_symbol_main      # SMA + FillTracking only
./build/src/engine/main                   # Single symbol, single strategy
```

## 📊 Strategy Performance Comparison

When you run all strategies, you'll get results files like:

```
results/
├── AAPL_SMA_20_50_[timestamp]_trades.csv      # SMA strategy trades
├── AAPL_FillTracking_[timestamp]_trades.csv   # Fill tracking trades
├── AAPL_DoNothing_[timestamp]_trades.csv      # Baseline (no trades)
├── GOOG_SMA_20_50_[timestamp]_trades.csv      # SMA strategy trades
├── GOOG_FillTracking_[timestamp]_trades.csv   # Fill tracking trades
├── GOOG_DoNothing_[timestamp]_trades.csv      # Baseline (no trades)
├── ... (other symbols)
├── PairsTrading_AAPL_GOOG_[timestamp]_trades.csv  # Pairs trading
└── performance_summary.csv                    # Overall comparison
```

## 🎯 Strategy Selection Guide

### For Beginners
- **Start with**: Fill Tracking Strategy (guaranteed to work)
- **Then try**: SMA Crossover (classic trend-following)
- **Baseline**: Do Nothing Strategy (performance comparison)

### For Advanced Users
- **Statistical arbitrage**: Pairs Trading Strategy
- **Custom strategies**: Extend the `IStrategy` interface
- **Risk management**: Use Moving Standard Deviation utilities

### For Testing
- **System verification**: Fill Tracking Strategy
- **Performance baseline**: Do Nothing Strategy
- **Framework testing**: All strategies together

## 🔧 Creating Custom Strategies

To create a new strategy:

1. **Create header file** (`include/qse/strategy/MyStrategy.h`):
```cpp
#pragma once
#include "qse/strategy/IStrategy.h"

namespace qse {
class MyStrategy : public IStrategy {
public:
    MyStrategy(OrderManager* order_manager, const std::string& symbol);
    
    void on_tick(const Tick& tick) override;
    void on_bar(const Bar& bar) override;
    void on_fill(const Fill& fill) override;

private:
    OrderManager* order_manager_;
    std::string symbol_;
    // Add your strategy-specific members
};
}
```

2. **Create implementation** (`src/strategy/MyStrategy.cpp`)
3. **Add to CMakeLists.txt**
4. **Add to multi-strategy engine**

## 📈 Strategy Analysis

After running strategies, analyze results:

```bash
# Analyze all strategies
python3 scripts/analysis/analyze_multi_strategy.py

# Compare specific strategies
python3 scripts/analysis/analyze.py

# Generate performance plots
python3 scripts/analysis/plot_current_run.py
```

## 🎯 Expected Results

### Fill Tracking Strategy
- **Guaranteed**: At least one trade per symbol
- **Purpose**: Verify system functionality
- **Performance**: Should show basic order execution

### SMA Crossover Strategy
- **Trades**: Depends on market conditions and data
- **Performance**: Trend-following returns
- **Risk**: Medium (trend reversals)

### Pairs Trading Strategy
- **Trades**: Statistical arbitrage opportunities
- **Performance**: Market-neutral returns
- **Risk**: Low (hedged positions)

### Do Nothing Strategy
- **Trades**: Zero (baseline)
- **Performance**: Cash returns only
- **Purpose**: Performance comparison baseline

## 🚀 Next Steps

1. **Run all strategies**: `./scripts/run_all_strategies.sh`
2. **Analyze results**: Check `results/` and `plots/` directories
3. **Compare performance**: Use analysis scripts
4. **Modify strategies**: Edit strategy files for customization
5. **Add new strategies**: Extend the framework

---

**Ready to run all your strategies? Execute: `./scripts/run_all_strategies.sh`** 🚀 