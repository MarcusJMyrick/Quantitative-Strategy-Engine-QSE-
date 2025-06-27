#!/bin/bash

# QSE All Strategies Runner
# Runs all available trading strategies: SMA Crossover, FillTracking, DoNothing (Baseline), and PairsTrading

set -e  # Exit on any error

echo "🚀 QSE All Strategies Runner"
echo "============================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    print_error "Please run this script from the QSE root directory"
    exit 1
fi

# Step 1: Check system status
print_status "Checking system status..."
./scripts/check_status.sh

# Step 2: Build the multi-strategy engine
print_status "Building multi-strategy engine..."

if [ ! -d "build" ]; then
    mkdir -p build
fi

cd build
cmake ..
make multi_strategy_engine -j$(nproc)
cd ..

if [ ! -f "build/multi_strategy_engine" ]; then
    print_error "Failed to build multi-strategy engine"
    exit 1
fi

print_success "Multi-strategy engine built successfully"

# Step 3: Check data availability
print_status "Checking data availability..."

# Check if we have the required raw tick files directly in data/
required_files=("data/raw_ticks_AAPL.csv" "data/raw_ticks_GOOG.csv" "data/raw_ticks_MSFT.csv" "data/raw_ticks_SPY.csv")
missing_files=()

for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        missing_files+=("$file")
    fi
done

if [ ${#missing_files[@]} -gt 0 ]; then
    print_error "Missing required data files:"
    for file in "${missing_files[@]}"; do
        print_error "  - $file"
    done
    print_error "Please ensure all required data files are present in the data/ directory."
    exit 1
fi

print_success "Data files ready"

# Step 4: Create results directory
print_status "Setting up results directory..."
mkdir -p results
mkdir -p plots

print_success "Directories ready"

# Step 5: Run all strategies
print_status "Starting multi-strategy engine..."

# Run the multi-strategy engine
./build/multi_strategy_engine

print_success "Multi-strategy engine completed!"

# Step 6: Analyze results
print_status "Analyzing results..."

# Run analysis script
python3 scripts/analysis/analyze_multi_strategy.py

print_success "Analysis complete!"

echo ""
echo "🎉 All Strategies Complete!"
echo "==========================="
echo "Strategies run:"
echo "  ✅ SMA Crossover (20/50)"
echo "  ✅ Fill Tracking (Smoke Test)"
echo "  ✅ Do Nothing (Baseline)"
echo "  ✅ Pairs Trading (AAPL vs GOOG)"
echo ""
echo "Results are available in:"
echo "  - results/ (trade logs and performance data)"
echo "  - plots/ (performance charts)"
echo ""
echo "To run again, simply execute: ./scripts/run_all_strategies.sh"
echo "To run individual strategies, see scripts/execution/"
echo "To analyze results, see scripts/analysis/" 