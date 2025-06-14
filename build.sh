#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure CMake
cmake -DCMAKE_PREFIX_PATH=/opt/homebrew ..

# Build the project
cmake --build .

# Generate test data
cd ..
python3 tests/create_test_data.py

# Run the tests
cd build
ctest --verbose 