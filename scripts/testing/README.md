# Testing Scripts

Scripts for testing, debugging, and performance evaluation.

## Scripts

### `test.sh`
**Run unit tests**
- Executes the full test suite
- Runs all C++ and Python tests
- Generates test reports

### `test_with_subset.py`
**Test with data subset**
- Runs tests with smaller datasets
- Faster execution for development
- Validates functionality without full data

### `test_distributed_system.sh`
**Test distributed components**
- Tests ZeroMQ messaging system
- Validates distributed backtesting
- Performance testing for scalability

### `performance_test.sh`
**Performance benchmarking**
- Measures execution time and memory usage
- Compares different configurations
- Identifies bottlenecks

### `debug_crash.sh`
**Debug crash scenarios**
- Sets up debugging environment
- Enables core dumps and logging
- Helps diagnose segmentation faults

### `direct_test.cpp`
**Direct C++ testing**
- Standalone C++ test program
- Tests specific components in isolation
- Useful for debugging C++ issues

## Usage

```bash
# Run all tests
./scripts/run.sh test quick

# Test with subset
./scripts/run.sh test subset

# Performance testing
./scripts/run.sh test perf
```

## Debugging

### Common Issues
1. **Segmentation Faults**: Use `debug_crash.sh`
2. **Memory Leaks**: Use `performance_test.sh`
3. **Build Issues**: Use `direct_test.cpp`

### Debug Environment
```bash
# Enable debugging
export QSE_DEBUG=1
export QSE_LOG_LEVEL=DEBUG

# Run with debug output
./scripts/testing/debug_crash.sh
```

## Dependencies

- gtest/gmock (C++ testing)
- pytest (Python testing)
- valgrind (memory checking) 