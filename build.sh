#!/bin/bash

# This script automates the entire workflow for the Quantitative Strategy Engine.
# It will exit immediately if any command fails.
set -e

# --- 1. Data Preparation (from project root) ---
echo "--- Preparing Data ---"
python3 scripts/download_data.py
python3 scripts/process_data.py
python3 tests/create_test_data.py
echo "--- Data Preparation Complete ---"
echo

# --- 2. Build C++ Project (inside build directory) ---
echo "--- Building C++ Project ---"
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
./build/qse_app
echo "--- Application Finished ---"