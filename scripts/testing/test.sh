#!/bin/bash

# This script is for quickly building the C++ project and running unit tests.
#
# The script will exit immediately if any command fails.
set -e

# --- Best Practice: Go to the project root directory ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# --- 1. Generate Test-Specific Data ---
echo "--- Generating required test data... ---"
# UPDATED PATH: The test data script has moved.
python3 "$PROJECT_ROOT/tests/python/create_test_data.py"
echo "--- Test data ready. ---"
echo

# --- 2. Build C++ Project ---
echo "--- Building C++ Project (Release Mode)... ---"
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew ..
cmake --build . -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
echo "--- Build Complete. ---"
echo

# --- 3. Run Unit Tests ---
echo "--- Running Unit Tests... ---"
ctest --verbose
echo "--- All Tests Passed Successfully! ---"