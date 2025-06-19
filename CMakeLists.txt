cmake_minimum_required(VERSION 3.14)
project(QSE VERSION 0.1.0 LANGUAGES CXX)

# Set build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build types" FORCE)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Set test data directory
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test_data" CACHE PATH "Directory containing test data files")

# Find Arrow and Parquet
# Note: Your build system is configured for Parquet files, which is great for performance.
# Our main.cpp is currently using the CSV reader, but the build will still work correctly.
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# Fetch and configure Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# --- Main library ---
# Adding source files using their new paths
add_library(qse SHARED
    src/core/Backtester.cpp
    src/core/ThreadPool.cpp
    src/data/BarBuilder.cpp
    src/data/CSVDataReader.cpp
    src/data/ParquetDataReader.cpp
    src/order/OrderManager.cpp
    src/strategy/MovingAverage.cpp
    src/strategy/SMACrossoverStrategy.cpp
)

# Link libraries to qse
target_link_libraries(qse PRIVATE 
    Arrow::arrow_shared 
    Parquet::parquet_shared
)

# Include directories for qse - updated to use new structure
target_include_directories(qse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Test executable ---
# Adding test files with updated paths
add_executable(run_tests
    tests/test_DataReader.cpp
    tests/test_OrderManager.cpp
    tests/test_Backtester.cpp
    tests/test_Strategy.cpp
    tests/test_ThreadPool.cpp
    tests/test_BarBuilder.cpp
)

# Link libraries to test executable (Preserving original structure)
target_link_libraries(run_tests PRIVATE 
    qse 
    gtest 
    gtest_main 
    gmock 
    gmock_main
)

# Include directories for tests - updated to use new structure
target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
)

# Add compile definition for test data directory
target_compile_definitions(run_tests PRIVATE 
    TEST_DATA_DIR="${TEST_DATA_DIR}"
)

# Add filesystem library for macOS
if(APPLE)
    target_link_libraries(run_tests PRIVATE "-lstdc++")
endif()

# Register tests
include(GoogleTest)
gtest_discover_tests(run_tests)

# Set CMAKE_PREFIX_PATH to include Homebrew directories
set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})

# --- Create the main application executable ---
add_executable(qse_app src/main.cpp)
target_link_libraries(qse_app PRIVATE qse Arrow::arrow_shared Parquet::parquet_shared)

# Include directories for main app
target_include_directories(qse_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)