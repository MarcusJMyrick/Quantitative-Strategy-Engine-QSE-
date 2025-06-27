#!/bin/bash

# QSE Quick Start Script
# This script sets up and runs the Quantitative Strategy Engine

set -e  # Exit on any error

echo "🚀 QSE Quick Start Script"
echo "=========================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
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

# Step 1: Check system dependencies
print_status "Checking system dependencies..."

# Check for required tools
command -v cmake >/dev/null 2>&1 || { print_error "cmake is required but not installed. Please install cmake."; exit 1; }
command -v make >/dev/null 2>&1 || { print_error "make is required but not installed. Please install make."; exit 1; }
command -v python3 >/dev/null 2>&1 || { print_error "python3 is required but not installed. Please install python3."; exit 1; }

print_success "System dependencies OK"

# Step 2: Setup Python environment
print_status "Setting up Python environment..."

if [ ! -d "venv" ]; then
    print_status "Creating virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
source venv/bin/activate

# Install Python dependencies
print_status "Installing Python dependencies..."
pip install --upgrade pip
pip install -r requirements.txt

print_success "Python environment ready"

# Step 3: Build the C++ engine
print_status "Building C++ engine..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
cmake ..
make -j$(nproc)

print_success "C++ engine built successfully"

# Go back to root
cd ..

# Step 4: Check data availability
print_status "Checking data availability..."

if [ ! -d "data/processed" ] || [ -z "$(ls -A data/processed 2>/dev/null)" ]; then
    print_warning "No processed data found. Running data processing..."
    
    # Run data processing
    python3 scripts/data/process_data.py
    
    if [ ! -d "data/processed" ] || [ -z "$(ls -A data/processed 2>/dev/null)" ]; then
        print_error "Data processing failed. Please check data/raw directory for input files."
        exit 1
    fi
fi

print_success "Data ready"

# Step 5: Create results directory
print_status "Setting up results directory..."
mkdir -p results
mkdir -p plots

print_success "Directories ready"

# Step 6: Run the engine
print_status "Starting QSE engine..."

# Run multi-symbol multi-strategy engine
./build/src/engine/multi_symbol_main

print_success "Engine completed!"

# Step 7: Analyze results
print_status "Analyzing results..."

# Run analysis script
python3 scripts/analysis/analyze_multi_strategy.py

print_success "Analysis complete!"

echo ""
echo "🎉 QSE Quick Start Complete!"
echo "============================="
echo "Results are available in:"
echo "  - results/ (trade logs and performance data)"
echo "  - plots/ (performance charts)"
echo ""
echo "To run again, simply execute: ./scripts/quick_start.sh"
echo "To run different configurations, see scripts/execution/"
echo "To analyze results, see scripts/analysis/" 