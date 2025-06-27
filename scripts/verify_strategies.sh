#!/bin/bash

# QSE Strategy Verification Script
# Verifies that strategies are running properly and performing trades

set -e

echo "🔍 QSE Strategy Verification"
echo "============================"

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

# Step 1: Check if engine is built
echo ""
echo "🔨 Step 1: Engine Build Status"
echo "-------------------------------"

if [ -f "build/multi_strategy_engine" ]; then
    print_ok "Multi-strategy engine is built"
else
    print_error "Multi-strategy engine not found. Building..."
    ./scripts/build/build.sh
fi

# Step 2: Check data availability
echo ""
echo "📊 Step 2: Data Availability"
echo "----------------------------"

data_files=("data/raw_ticks_AAPL.csv" "data/raw_ticks_GOOG.csv" "data/raw_ticks_MSFT.csv" "data/raw_ticks_SPY.csv")
all_data_ok=true

for file in "${data_files[@]}"; do
    if [ -f "$file" ]; then
        lines=$(wc -l < "$file")
        if [ $lines -gt 100 ]; then
            print_ok "$(basename $file): $lines lines"
        else
            print_warning "$(basename $file): Only $lines lines (may be insufficient)"
            all_data_ok=false
        fi
    else
        print_error "$(basename $file): Not found"
        all_data_ok=false
    fi
done

if [ "$all_data_ok" = false ]; then
    print_warning "Some data files may be insufficient for meaningful testing"
fi

# Step 3: Run a test with debug output
echo ""
echo "🚀 Step 3: Running Test with Debug Output"
echo "----------------------------------------"

# Create a test script that runs with verbose output
cat > test_debug.cpp << 'EOF'
#include <iostream>
#include <memory>
#include "qse/data/CSVDataReader.h"
#include "qse/strategy/FillTrackingStrategy.h"
#include "qse/order/OrderManager.h"
#include "qse/core/Backtester.h"

int main() {
    std::cout << "=== DEBUG TEST STARTING ===" << std::endl;
    
    try {
        // Test with AAPL data
        std::string data_file = "data/raw_ticks_AAPL.csv";
        std::cout << "Loading data from: " << data_file << std::endl;
        
        auto data_reader = std::make_unique<qse::CSVDataReader>(data_file);
        std::cout << "Data reader created successfully" << std::endl;
        
        auto order_manager = std::make_shared<qse::OrderManager>(
            100000.0, 
            "test_debug_equity.csv", 
            "test_debug_trades.csv"
        );
        std::cout << "Order manager created successfully" << std::endl;
        
        auto strategy = std::make_unique<qse::FillTrackingStrategy>(order_manager);
        std::cout << "FillTracking strategy created successfully" << std::endl;
        
        qse::Backtester backtester(
            "AAPL",
            std::move(data_reader),
            std::move(strategy),
            order_manager,
            std::chrono::seconds(60)
        );
        std::cout << "Backtester created successfully" << std::endl;
        
        std::cout << "Starting backtest..." << std::endl;
        backtester.run();
        std::cout << "Backtest completed successfully" << std::endl;
        
        // Check if files were created and have content
        std::ifstream equity_file("test_debug_equity.csv");
        std::ifstream trades_file("test_debug_trades.csv");
        
        if (equity_file.good()) {
            std::string line;
            int lines = 0;
            while (std::getline(equity_file, line)) lines++;
            std::cout << "Equity file created with " << lines << " lines" << std::endl;
        } else {
            std::cout << "WARNING: Equity file not created or empty" << std::endl;
        }
        
        if (trades_file.good()) {
            std::string line;
            int lines = 0;
            while (std::getline(trades_file, line)) lines++;
            std::cout << "Trades file created with " << lines << " lines" << std::endl;
        } else {
            std::cout << "WARNING: Trades file not created or empty" << std::endl;
        }
        
        std::cout << "=== DEBUG TEST COMPLETED ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
EOF

# Build the debug test
echo "Building debug test..."
cd build
cmake ..
make -j$(nproc)
cd ..

if [ -f "build/test_debug" ]; then
    print_ok "Debug test built successfully"
    
    echo "Running debug test..."
    ./build/test_debug
    
    # Check results
    if [ -f "test_debug_trades.csv" ]; then
        trades=$(wc -l < test_debug_trades.csv)
        if [ $trades -gt 1 ]; then
            print_ok "Debug test generated $((trades-1)) trades"
            echo "Sample trade:"
            head -2 test_debug_trades.csv
        else
            print_warning "Debug test generated no trades"
        fi
    else
        print_error "Debug test did not create trades file"
    fi
    
    # Clean up test files
    rm -f test_debug_equity.csv test_debug_trades.csv
    rm -f test_debug.cpp
else
    print_error "Failed to build debug test"
fi

# Step 4: Check existing results
echo ""
echo "📈 Step 4: Existing Results Analysis"
echo "-----------------------------------"

if [ -f "results/tradelog_AAPL.csv" ]; then
    trades=$(wc -l < results/tradelog_AAPL.csv)
    if [ $trades -gt 1 ]; then
        print_ok "Existing AAPL results: $((trades-1)) trades"
        echo "Sample trades:"
        head -3 results/tradelog_AAPL.csv
    else
        print_warning "Existing AAPL results: No trades"
    fi
else
    print_warning "No existing AAPL results found"
fi

# Step 5: Recommendations
echo ""
echo "💡 Step 5: Recommendations"
echo "-------------------------"

echo "To ensure strategies are running properly:"
echo ""
echo "1. Run with debug output:"
echo "   ./scripts/execution/dev_mode.sh"
echo ""
echo "2. Check strategy logic:"
echo "   - FillTracking: Should generate 1 trade per symbol"
echo "   - SMA: May not generate trades with short data"
echo "   - PairsTrading: May need more data for signals"
echo ""
echo "3. Monitor real-time output:"
echo "   - Look for strategy decision messages"
echo "   - Check for order submission messages"
echo "   - Verify fill events"
echo ""
echo "4. Verify data quality:"
echo "   - Ensure sufficient price movement for SMA crossovers"
echo "   - Check data format and timestamps"
echo "   - Verify symbol names match strategy expectations"

echo ""
echo "🎯 Verification Complete!" 