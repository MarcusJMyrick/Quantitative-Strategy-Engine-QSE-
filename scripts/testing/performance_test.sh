#!/bin/bash

# Performance test script - measures actual processing speed
set -e

# Source build utilities for safety features
source "$(dirname "$0")/build_utils.sh"

# Go to project root
cd "$(dirname "$0")/.."

echo "🚀 Performance Test - Direct CSV Processing"
echo "=========================================="
echo

# --- Safety Checks ---
acquire_lock "performance_test.sh"
check_running_processes

# --- 1. Build C++ Project ---
echo "🔨 Building C++ Project..."
PREFIX_PATH_ARG=""
if [ -d /opt/homebrew ]; then
  PREFIX_PATH_ARG="-DCMAKE_PREFIX_PATH=/opt/homebrew"
fi

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release $PREFIX_PATH_ARG ..
cmake --build . -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
echo "✅ Build Complete"
echo

# --- 2. Check test data ---
cd ..
if [ ! -f "test_data/test_raw_ticks_SPY.csv" ]; then
    echo "📝 Creating test data subsets..."
    python3 scripts/test_with_subset.py
    echo
fi

# --- 3. Run Performance Test ---
echo "⚡ Running Direct Performance Test..."
echo "Processing 1,000 ticks directly from CSV (no ZeroMQ overhead)"
echo

time ./build/direct_test

echo
echo "📊 Performance Analysis:"
echo "======================="
echo "If this is still slow (>10 seconds), the bottleneck is in:"
echo "• Bar building logic"
echo "• Strategy calculations" 
echo "• Order book operations"
echo "• Debug logging overhead"
echo
echo "If this is fast (<5 seconds), the bottleneck was ZeroMQ messaging."
echo
echo "--- Performance Test Complete ---" 