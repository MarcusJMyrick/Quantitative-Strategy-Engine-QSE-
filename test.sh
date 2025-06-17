#!/bin/bash

# This script is for quickly building the C++ project and running unit tests.
# It does NOT re-download market data, making it much faster for C++ development.
#
# The script will exit immediately if any command fails.
set -e

# --- 1. Generate Test-Specific Data ---
# This ensures that any small data files needed *only* for the tests are present.
echo "--- Generating required test data... ---"
python3 tests/create_test_data.py
echo "--- Test data ready. ---"
echo

# --- 2. Build C++ Project ---
echo "--- Building C++ Project (Release Mode)... ---"
# Create the build directory if it doesn't already exist.
mkdir -p build
cd build

# Configure CMake.
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew ..

# Build the project using all available CPU cores for speed.
cmake --build . -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo "--- Build Complete. ---"
echo

# --- 3. Run Unit Tests ---
echo "--- Running Unit Tests... ---"
# Execute all tests discovered by CTest to verify correctness.
ctest --verbose
echo "--- All Tests Passed Successfully! ---"