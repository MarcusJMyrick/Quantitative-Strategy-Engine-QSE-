#!/bin/bash

# Integration test script for the distributed QSE system
# This script tests the complete data publisher -> strategy engine pipeline

set -e  # Exit on any error

echo "=== QSE Distributed System Integration Test ==="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
DATA_FILE="data/raw_ticks_AAPL.csv"
PUBLISHER_PORT="5555"
TEST_TIMEOUT=30  # seconds

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    print_error "Build directory not found. Please run 'mkdir build && cd build && cmake .. && make' first."
    exit 1
fi

# Check if data file exists
if [ ! -f "$DATA_FILE" ]; then
    print_warning "Data file $DATA_FILE not found. Creating a small test file..."
    
    # Create a small test CSV file
    mkdir -p data
    cat > "$DATA_FILE" << EOF
timestamp,price,volume
1640995200,150.25,1000
1640995260,150.50,1200
1640995320,150.75,1100
1640995380,151.00,1300
1640995440,151.25,1400
EOF
    print_status "Created test data file: $DATA_FILE"
fi

# Function to cleanup background processes
cleanup() {
    print_status "Cleaning up background processes..."
    if [ ! -z "$PUBLISHER_PID" ]; then
        kill $PUBLISHER_PID 2>/dev/null || true
    fi
    if [ ! -z "$ENGINE_PID" ]; then
        kill $ENGINE_PID 2>/dev/null || true
    fi
}

# Set up cleanup on script exit
trap cleanup EXIT

# Start the data publisher in the background
print_status "Starting data publisher..."
cd "$BUILD_DIR"
./data_publisher "../$DATA_FILE" > publisher.log 2>&1 &
PUBLISHER_PID=$!

# Wait for publisher to initialize
print_status "Waiting for publisher to initialize..."
sleep 2

# Check if publisher is still running
if ! kill -0 $PUBLISHER_PID 2>/dev/null; then
    print_error "Publisher failed to start. Check publisher.log for details."
    exit 1
fi

print_status "Publisher started successfully (PID: $PUBLISHER_PID)"

# Start the strategy engine in the background
print_status "Starting strategy engine..."
./strategy_engine "tcp://localhost:$PUBLISHER_PORT" > engine.log 2>&1 &
ENGINE_PID=$!

# Wait for engine to initialize
print_status "Waiting for strategy engine to initialize..."
sleep 2

# Check if engine is still running
if ! kill -0 $ENGINE_PID 2>/dev/null; then
    print_error "Strategy engine failed to start. Check engine.log for details."
    exit 1
fi

print_status "Strategy engine started successfully (PID: $ENGINE_PID)"

# Wait for the system to complete processing
print_status "Waiting for system to complete processing (timeout: ${TEST_TIMEOUT}s)..."
TIMEOUT_COUNT=0
while [ $TIMEOUT_COUNT -lt $TEST_TIMEOUT ]; do
    # Check if both processes are still running
    if ! kill -0 $PUBLISHER_PID 2>/dev/null; then
        print_status "Publisher completed"
        break
    fi
    
    if ! kill -0 $ENGINE_PID 2>/dev/null; then
        print_status "Strategy engine completed"
        break
    fi
    
    sleep 1
    TIMEOUT_COUNT=$((TIMEOUT_COUNT + 1))
done

# Check final status
if [ $TIMEOUT_COUNT -ge $TEST_TIMEOUT ]; then
    print_warning "Test timed out after ${TEST_TIMEOUT} seconds"
fi

# Wait a bit more for any final processing
sleep 2

# Check logs for success indicators
print_status "Analyzing results..."

PUBLISHER_SUCCESS=false
ENGINE_SUCCESS=false

# Check publisher log
if grep -q "Data broadcast completed" publisher.log; then
    PUBLISHER_SUCCESS=true
    print_status "✓ Publisher completed successfully"
else
    print_warning "⚠ Publisher may not have completed properly"
fi

# Check engine log
if grep -q "Backtest completed successfully" engine.log; then
    ENGINE_SUCCESS=true
    print_status "✓ Strategy engine completed successfully"
else
    print_warning "⚠ Strategy engine may not have completed properly"
fi

# Check for errors
if grep -q "Error\|error\|ERROR" publisher.log; then
    print_error "✗ Errors found in publisher log"
    PUBLISHER_SUCCESS=false
fi

if grep -q "Error\|error\|ERROR" engine.log; then
    print_error "✗ Errors found in engine log"
    ENGINE_SUCCESS=false
fi

# Print summary
echo ""
echo "=== Test Summary ==="
echo "Publisher: $([ "$PUBLISHER_SUCCESS" = true ] && echo "✓ PASS" || echo "✗ FAIL")"
echo "Engine: $([ "$ENGINE_SUCCESS" = true ] && echo "✓ PASS" || echo "✗ FAIL")"

if [ "$PUBLISHER_SUCCESS" = true ] && [ "$ENGINE_SUCCESS" = true ]; then
    print_status "🎉 Integration test PASSED!"
    echo ""
    echo "=== Final Results ==="
    echo "Publisher log:"
    tail -10 publisher.log
    echo ""
    echo "Engine log:"
    tail -10 engine.log
    exit 0
else
    print_error "❌ Integration test FAILED!"
    echo ""
    echo "=== Debug Information ==="
    echo "Publisher log:"
    cat publisher.log
    echo ""
    echo "Engine log:"
    cat engine.log
    exit 1
fi 