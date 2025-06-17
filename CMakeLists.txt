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
# Adding ThreadPool.cpp to the main qse library
add_library(qse SHARED
    src/CSVDataReader.cpp
    src/OrderManager.cpp
    src/Backtester.cpp
    src/SMACrossoverStrategy.cpp
    src/MovingAverage.cpp
    src/ThreadPool.cpp
    src/ParquetDataReader.cpp
)

# Link libraries to qse
target_link_libraries(qse PRIVATE 
    Arrow::arrow_shared 
    Parquet::parquet_shared
)

# Include directories for qse
target_include_directories(qse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Test executable ---
# Adding test_ThreadPool.cpp to the test executable
add_executable(run_tests
    tests/test_DataReader.cpp
    tests/test_OrderManager.cpp
    tests/test_Backtester.cpp
    tests/test_Strategy.cpp
    tests/test_ThreadPool.cpp
)

# Link libraries to test executable (Preserving original structure)
target_link_libraries(run_tests PRIVATE 
    qse 
    gtest 
    gtest_main 
    gmock 
    gmock_main
)

# Include directories for tests
target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Create the main application executable ---
add_executable(qse_app src/main.cpp)
target_link_libraries(qse_app PRIVATE qse Arrow::arrow_shared Parquet::parquet_shared)

set(SOURCES
    src/main.cpp
    src/Backtester.cpp
    src/CSVDataReader.cpp
    src/ParquetDataReader.cpp
    src/OrderManager.cpp
    src/SMACrossoverStrategy.cpp
    src/ThreadPool.cpp
)

set(HEADERS
    include/Backtester.h
    include/IDataReader.h
    include/IStrategy.h
    include/IOrderManager.h
    include/CSVDataReader.h
    include/ParquetDataReader.h
    include/OrderManager.h
    include/SMACrossoverStrategy.h
    include/ThreadPool.h
)