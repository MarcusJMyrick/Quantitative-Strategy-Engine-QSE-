# 🚀 QSE - Quantitative Strategy Engine

## Quick Start (Recommended)

For the fastest way to get everything running:

```bash
./scripts/quick_start.sh
```

This single command will:
- ✅ Check system dependencies
- ✅ Setup Python virtual environment
- ✅ Build the C++ engine
- ✅ Process data (if needed)
- ✅ Run the multi-symbol multi-strategy engine
- ✅ Analyze results and generate plots

## 📋 Prerequisites

### System Requirements
- **macOS/Linux** (tested on macOS 24.5.0)
- **CMake** (3.10+)
- **Make** (GNU make)
- **Python 3.8+**
- **C++17 compatible compiler** (GCC 7+, Clang 5+)

### Install Dependencies

**On macOS:**
```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required tools
brew install cmake python3
```

**On Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake build-essential python3 python3-venv python3-pip
```

## 🛠️ Manual Setup (Alternative)

If you prefer step-by-step control:

### 1. Setup Python Environment
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 2. Build C++ Engine
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..
```

### 3. Process Data
```bash
python3 scripts/data/process_data.py
```

### 4. Run Engine
```bash
./build/src/engine/multi_symbol_main
```

### 5. Analyze Results
```bash
python3 scripts/analysis/analyze_multi_strategy.py
```

## 📁 Available Scripts

### 🚀 Execution Scripts
| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/quick_start.sh` | **Complete setup and run** | `./scripts/quick_start.sh` |
| `scripts/execution/run_multi_symbol.sh` | Multi-symbol trading | `./scripts/execution/run_multi_symbol.sh` |
| `scripts/execution/dev_mode.sh` | Development mode with debug output | `./scripts/execution/dev_mode.sh` |
| `scripts/execution/estimate_runtime.py` | Estimate execution time | `python3 scripts/execution/estimate_runtime.py` |

### 🔧 Build Scripts
| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/build/build.sh` | Standard build | `./scripts/build/build.sh` |
| `scripts/build/quick_build.sh` | Fast build (incremental) | `./scripts/build/quick_build.sh` |
| `scripts/build/ultra_quick_build.sh` | Minimal build for testing | `./scripts/build/ultra_quick_build.sh` |

### 📊 Analysis Scripts
| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/analysis/analyze_multi_strategy.py` | **Main analysis** | `python3 scripts/analysis/analyze_multi_strategy.py` |
| `scripts/analysis/analyze.py` | Single strategy analysis | `python3 scripts/analysis/analyze.py` |
| `scripts/analysis/analyze_multi_run.py` | Compare multiple runs | `python3 scripts/analysis/analyze_multi_run.py` |
| `scripts/analysis/plot_current_run.py` | Quick plotting | `python3 scripts/analysis/plot_current_run.py` |
| `scripts/analysis/find_pairs.py` | Pairs trading analysis | `python3 scripts/analysis/find_pairs.py` |

### 🧪 Testing Scripts
| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/testing/test.sh` | Run all tests | `./scripts/testing/test.sh` |
| `scripts/testing/performance_test.sh` | Performance benchmarks | `./scripts/testing/performance_test.sh` |
| `scripts/testing/debug_crash.sh` | Debug crashes | `./scripts/testing/debug_crash.sh` |
| `scripts/testing/test_with_subset.py` | Test with data subset | `python3 scripts/testing/test_with_subset.py` |

### 📁 Data Scripts
| Script | Purpose | Usage |
|--------|---------|-------|
| `scripts/data/process_data.py` | Process raw data | `python3 scripts/data/process_data.py` |
| `scripts/data/download_data.py` | Download market data | `python3 scripts/data/download_data.py` |
| `scripts/data/organize_results.py` | Organize output files | `python3 scripts/data/organize_results.py` |
| `scripts/data/organize_plots.py` | Organize plot files | `python3 scripts/data/organize_plots.py` |

## 🎯 Common Use Cases

### 1. **First Time Setup & Run**
```bash
./scripts/quick_start.sh
```

### 2. **Development Mode (with debug output)**
```bash
./scripts/execution/dev_mode.sh
```

### 3. **Quick Rebuild & Test**
```bash
./scripts/build/quick_build.sh
./scripts/testing/test.sh
```

### 4. **Performance Testing**
```bash
./scripts/testing/performance_test.sh
```

### 5. **Analyze Previous Results**
```bash
python3 scripts/analysis/analyze_multi_strategy.py
```

### 6. **Compare Multiple Runs**
```bash
python3 scripts/analysis/analyze_multi_run.py
```

## 📊 Understanding Output

### Results Directory Structure
```
results/
├── AAPL_SMA_20_50_trades.csv      # SMA strategy trades for AAPL
├── AAPL_FillTracking_trades.csv   # FillTracking strategy trades for AAPL
├── GOOGL_SMA_20_50_trades.csv     # SMA strategy trades for GOOGL
├── GOOGL_FillTracking_trades.csv  # FillTracking strategy trades for GOOGL
├── ... (other symbols)
└── performance_summary.csv        # Overall performance metrics
```

### Plots Directory Structure
```
plots/
├── performance_charts/            # Performance visualization
├── trade_analysis/               # Trade pattern analysis
├── strategy_comparison/          # Strategy comparison charts
└── risk_metrics/                 # Risk analysis plots
```

## 🔧 Configuration

### Main Configuration (`config.yaml`)
- **Symbols**: Trading symbols with slippage settings
- **Backtester**: Initial cash, commission rates
- **Data**: File paths and processing settings

### Strategy Configuration
Strategies are configured in the C++ code:
- **SMA Crossover**: 20/50 period moving averages
- **Fill Tracking**: Guaranteed trade generation for testing
- **Pairs Trading**: Statistical arbitrage between correlated assets

## 🐛 Troubleshooting

### Common Issues

**1. Build Errors**
```bash
# Clean and rebuild
rm -rf build
./scripts/build/build.sh
```

**2. No Trades Generated**
```bash
# Check data availability
ls -la data/processed/
# Run with debug output
./scripts/execution/dev_mode.sh
```

**3. Python Import Errors**
```bash
# Reinstall dependencies
source venv/bin/activate
pip install -r requirements.txt
```

**4. Permission Denied**
```bash
# Make scripts executable
chmod +x scripts/*.sh
chmod +x scripts/*/*.sh
```

### Debug Mode
For detailed debugging:
```bash
./scripts/execution/dev_mode.sh
```

This will show:
- Tick processing
- Strategy decisions
- Order submissions
- Fill events

## 📈 Performance Optimization

### Build Optimization
```bash
# Use all CPU cores for building
make -j$(nproc)
```

### Runtime Optimization
- The engine uses multi-threading for data processing
- ZeroMQ messaging for high-performance communication
- Memory-mapped files for large datasets

## 🎯 Next Steps

1. **Run the quick start**: `./scripts/quick_start.sh`
2. **Review results**: Check `results/` and `plots/` directories
3. **Modify strategies**: Edit strategy files in `src/strategy/`
4. **Add new symbols**: Update `config.yaml` and add data files
5. **Customize analysis**: Modify analysis scripts in `scripts/analysis/`

## 📞 Support

If you encounter issues:
1. Check the troubleshooting section above
2. Run in debug mode: `./scripts/execution/dev_mode.sh`
3. Review the test output: `./scripts/testing/test.sh`
4. Check the logs in `results/` directory

---

**Happy Trading! 🚀📈** 