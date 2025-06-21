#!/bin/bash

# This script automates the entire workflow for the Quantitative Strategy Engine.
# It will exit immediately if any command fails.
set -e

# --- Best Practice: Go to the project root directory ---
# This makes sure the script can be run from anywhere.
cd "$(dirname "$0")/.."

# --- 1. Data Preparation (from project root) ---
echo "--- Preparing Data ---"
python3 scripts/download_data.py
python3 scripts/process_data.py
# UPDATED PATH: The test data script has moved.
python3 tests/python/create_test_data.py
echo "--- Data Preparation Complete ---"
echo

# --- 2. Build C++ Project (inside build directory) ---
echo "--- Building C++ Project ---"
# The rest of the build logic is fine. It uses the standard out-of-source build.
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew ..
cmake --build . -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
echo "--- Build Complete ---"
echo

# --- 3. Run Unit Tests (inside build directory) ---
echo "--- Running Unit Tests ---"
ctest --verbose
echo "--- Unit Tests Passed ---"
echo

# --- 4. Run Main Application (from project root) ---
echo "--- Running Multi-Asset Backtest Application ---"
# IMPORTANT: Go back to the root directory before running the app.
cd ..
# CORRECTED NAME: The executable is named 'strategy_engine' in CMakeLists.txt.
./build/strategy_engine
echo "--- Application Finished ---"