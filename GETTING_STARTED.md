# 🚀 Getting Started with QSE

## Quick Start (One Command)

```bash
./scripts/quick_start.sh
```

This will set up everything and run the complete system!

## 📋 What You Have

Based on the status check, your system is **mostly ready**:

✅ **System Dependencies**: CMake, Make, Python3  
✅ **Python Environment**: Virtual environment active  
✅ **Build Directory**: Ready for compilation  
✅ **Results**: 45 result files from previous runs  
✅ **Configuration**: `config.yaml` properly configured  

## 🔧 What You Need to Do

### Option 1: Complete Setup (Recommended)
```bash
./scripts/quick_start.sh
```

### Option 2: Step by Step
```bash
# 1. Build the engine
./scripts/build/build.sh

# 2. Process data (if needed)
python3 scripts/data/process_data.py

# 3. Run the engine
./build/src/engine/multi_symbol_main

# 4. Analyze results
python3 scripts/analysis/analyze_multi_strategy.py
```

## 📁 Key Scripts You Need

### 🚀 **Main Scripts**
| Script | What it does | When to use |
|--------|-------------|-------------|
| `./scripts/quick_start.sh` | **Complete setup & run** | **First time or full reset** |
| `./scripts/check_status.sh` | Check system readiness | **Always run this first** |
| `./scripts/build/build.sh` | Build C++ engine | When you modify C++ code |
| `./scripts/analysis/analyze_multi_strategy.py` | Analyze results | After running the engine |

### 🔧 **Utility Scripts**
| Script | Purpose |
|--------|---------|
| `./scripts/execution/dev_mode.sh` | Run with debug output |
| `./scripts/testing/test.sh` | Run all tests |
| `./scripts/data/process_data.py` | Process raw data |
| `./scripts/analysis/plot_current_run.py` | Quick plotting |

## 🎯 Your Next Steps

1. **Check Status**: `./scripts/check_status.sh`
2. **Run Everything**: `./scripts/quick_start.sh`
3. **View Results**: Check `results/` and `plots/` directories
4. **Analyze**: `python3 scripts/analysis/analyze_multi_strategy.py`

## 📊 Understanding Output

### Results Files
- `results/AAPL_SMA_20_50_trades.csv` - SMA strategy trades for AAPL
- `results/AAPL_FillTracking_trades.csv` - FillTracking strategy trades for AAPL
- `results/performance_summary.csv` - Overall performance metrics

### Plots
- `plots/performance_charts/` - Performance visualization
- `plots/trade_analysis/` - Trade pattern analysis
- `plots/strategy_comparison/` - Strategy comparison charts

## 🐛 Troubleshooting

### If `quick_start.sh` fails:
```bash
# Check what's wrong
./scripts/check_status.sh

# Manual build
./scripts/build/build.sh

# Check for errors
./scripts/testing/test.sh
```

### If no trades are generated:
```bash
# Run with debug output
./scripts/execution/dev_mode.sh

# Check data availability
ls -la data/processed/
```

### If Python errors:
```bash
# Reinstall dependencies
source venv/bin/activate
pip install -r requirements.txt
```

## 🎉 Success Indicators

You'll know everything is working when:
- ✅ Engine runs without errors
- ✅ Results files are created in `results/`
- ✅ Plots are generated in `plots/`
- ✅ Analysis script completes successfully

## 📞 Need Help?

1. Run: `./scripts/check_status.sh`
2. Check: `RUN_ME.md` for detailed documentation
3. Debug: `./scripts/execution/dev_mode.sh`
4. Test: `./scripts/testing/test.sh`

---

**Ready to start? Run: `./scripts/quick_start.sh`** 🚀 