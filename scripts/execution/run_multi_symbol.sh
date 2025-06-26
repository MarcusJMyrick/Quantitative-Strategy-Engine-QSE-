#!/bin/bash

echo "=== Multi-Symbol Strategy Engine Runner ==="
echo "This script will:"
echo "1. Build the multi-symbol strategy engine"
echo "2. Run strategies for AAPL, GOOG, MSFT, and SPY"
echo "3. Generate comprehensive analysis and charts"
echo

# Function to check if command was successful
check_success() {
    if [ $? -ne 0 ]; then
        echo "❌ Error: $1 failed"
        exit 1
    fi
    echo "✅ $1 completed successfully"
}

# Step 1: Build the project
echo "🔨 Building multi-symbol strategy engine..."
cd "$(dirname "$0")/.."  # Go to project root
cmake --build build --target multi_symbol_engine
check_success "Build"

# Step 2: Check if data files exist
echo
echo "📊 Checking data files..."
for symbol in AAPL GOOG MSFT SPY; do
    if [ ! -f "data/raw_ticks_${symbol}.csv" ]; then
        echo "❌ Error: data/raw_ticks_${symbol}.csv not found"
        echo "Please run the data download script first"
        exit 1
    fi
    echo "✅ Found data/raw_ticks_${symbol}.csv"
done

# Step 3: Create results directory if it doesn't exist
mkdir -p results
mkdir -p plots

# Step 4: Run the multi-symbol strategy engine
echo
echo "🚀 Running multi-symbol strategy engine..."
./build/multi_symbol_engine
check_success "Strategy execution"

# Step 5: Extract timestamp from the output
echo
echo "📈 Analyzing results and generating charts..."

# Get the most recent timestamp from results directory
TIMESTAMP=$(ls results/equity_*_*.csv | head -1 | sed 's/.*_\([0-9_]*\)\.csv/\1/')

if [ -z "$TIMESTAMP" ]; then
    echo "❌ Error: Could not find timestamp from results files"
    exit 1
fi

echo "Found timestamp: $TIMESTAMP"

# Step 6: Run analysis script
python3 scripts/analyze_multi_run.py "$TIMESTAMP"
check_success "Analysis and chart generation"

# Step 7: Open results
echo
echo "🎉 Multi-symbol strategy run complete!"
echo
echo "📁 Results saved with timestamp: $TIMESTAMP"
echo "📊 Charts generated in plots/ directory"
echo

# List generated files
echo "Generated files:"
echo "Results:"
for symbol in AAPL GOOG MSFT SPY; do
    if [ -f "results/equity_${symbol}_${TIMESTAMP}.csv" ]; then
        echo "  ✅ results/equity_${symbol}_${TIMESTAMP}.csv"
    fi
    if [ -f "results/tradelog_${symbol}_${TIMESTAMP}.csv" ]; then
        echo "  ✅ results/tradelog_${symbol}_${TIMESTAMP}.csv"
    fi
done

echo
echo "Charts:"
if [ -f "plots/multi_symbol_summary_${TIMESTAMP}.png" ]; then
    echo "  📊 plots/multi_symbol_summary_${TIMESTAMP}.png"
fi
if [ -f "plots/performance_comparison_${TIMESTAMP}.png" ]; then
    echo "  📊 plots/performance_comparison_${TIMESTAMP}.png"
fi
for symbol in AAPL GOOG MSFT SPY; do
    if [ -f "plots/equity_curve_${symbol}_${TIMESTAMP}.png" ]; then
        echo "  📊 plots/equity_curve_${symbol}_${TIMESTAMP}.png"
    fi
done

echo
echo "🖼️  Opening plots directory..."
open plots/

echo
echo "🎯 Quick command to view main summary:"
echo "open plots/multi_symbol_summary_${TIMESTAMP}.png" 