#!/bin/bash

# QSE Status Check Script
# Verifies all components are ready to run

echo "🔍 QSE Status Check"
echo "==================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_ok() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# Check system dependencies
echo ""
echo "📋 System Dependencies:"
echo "----------------------"

if command -v cmake >/dev/null 2>&1; then
    print_ok "CMake: $(cmake --version | head -n1)"
else
    print_error "CMake not found"
fi

if command -v make >/dev/null 2>&1; then
    print_ok "Make: $(make --version | head -n1)"
else
    print_error "Make not found"
fi

if command -v python3 >/dev/null 2>&1; then
    print_ok "Python3: $(python3 --version)"
else
    print_error "Python3 not found"
fi

# Check Python environment
echo ""
echo "🐍 Python Environment:"
echo "---------------------"

if [ -d "venv" ]; then
    print_ok "Virtual environment exists"
    
    if [ -f "venv/bin/activate" ]; then
        print_ok "Virtual environment is valid"
    else
        print_error "Virtual environment is corrupted"
    fi
else
    print_warning "Virtual environment not found"
fi

if [ -f "requirements.txt" ]; then
    print_ok "Requirements file exists"
else
    print_error "Requirements file missing"
fi

# Check build status
echo ""
echo "🔨 Build Status:"
echo "---------------"

if [ -d "build" ]; then
    print_ok "Build directory exists"
    
    if [ -f "build/src/engine/multi_symbol_main" ]; then
        print_ok "Main executable exists"
    else
        print_warning "Main executable not found - needs building"
    fi
    
    if [ -f "build/src/engine/main" ]; then
        print_ok "Single symbol executable exists"
    else
        print_warning "Single symbol executable not found"
    fi
else
    print_warning "Build directory not found - needs building"
fi

# Check data availability
echo ""
echo "📊 Data Status:"
echo "---------------"

if [ -d "data" ]; then
    print_ok "Data directory exists"
    
    # Check for raw tick files directly in data/
    raw_tick_files=$(ls data/raw_ticks_*.csv 2>/dev/null | wc -l)
    if [ $raw_tick_files -gt 0 ]; then
        print_ok "Raw tick files: $raw_tick_files"
    else
        print_warning "No raw tick files found in data/"
    fi
    
    # Check for parquet files
    parquet_files=$(ls data/*.parquet 2>/dev/null | wc -l)
    if [ $parquet_files -gt 0 ]; then
        print_ok "Parquet files: $parquet_files"
    else
        print_info "No parquet files found"
    fi
    
    # Check for other CSV files
    other_csv_files=$(ls data/*.csv 2>/dev/null | wc -l)
    if [ $other_csv_files -gt 0 ]; then
        print_ok "Other CSV files: $other_csv_files"
    else
        print_info "No other CSV files found"
    fi
else
    print_error "Data directory not found"
fi

# Check results directory
echo ""
echo "📈 Results Status:"
echo "------------------"

if [ -d "results" ]; then
    print_ok "Results directory exists"
    
    result_files=$(ls results/*.csv 2>/dev/null | wc -l)
    if [ $result_files -gt 0 ]; then
        print_ok "Result files: $result_files"
    else
        print_info "No result files yet - run the engine first"
    fi
else
    print_info "Results directory not found - will be created on first run"
fi

if [ -d "plots" ]; then
    print_ok "Plots directory exists"
else
    print_info "Plots directory not found - will be created on first run"
fi

# Check configuration
echo ""
echo "⚙️  Configuration:"
echo "-----------------"

if [ -f "config.yaml" ]; then
    print_ok "Main config file exists"
else
    print_error "Main config file missing"
fi

# Summary
echo ""
echo "📋 Summary:"
echo "-----------"

if [ -d "build" ] && [ -f "build/multi_strategy_engine" ] && [ -f "data/raw_ticks_AAPL.csv" ]; then
    print_ok "Ready to run! Use: ./scripts/run_all_strategies.sh"
elif [ -d "build" ] && [ -f "build/multi_strategy_engine" ]; then
    print_warning "Engine built but missing data files. Check data/ directory."
elif [ -d "build" ]; then
    print_warning "Build directory exists but executable missing. Run: ./scripts/build/build.sh"
else
    print_warning "Not ready. Run: ./scripts/run_all_strategies.sh"
fi

echo ""
echo "🚀 Quick Commands:"
echo "------------------"
echo "  Setup & Run:     ./scripts/run_all_strategies.sh"
echo "  Build Only:      ./scripts/build/build.sh"
echo "  Run Engine:      ./build/multi_strategy_engine"
echo "  Analyze:         python3 scripts/analysis/analyze_multi_strategy.py"
echo "  Test:            ./scripts/testing/test.sh" 