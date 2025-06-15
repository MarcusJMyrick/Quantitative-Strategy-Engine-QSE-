cmake_minimum_required(VERSION 3.14)
project(QSE VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Set test data directory
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test_data" CACHE PATH "Directory containing test data files")

# Find Arrow and Parquet
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

# Main library
add_library(qse SHARED
    src/DataReader.cpp
    src/OrderManager.cpp
    src/Backtester.cpp
    # Add other source files here
)

# Link Arrow and Parquet to main library
target_link_libraries(qse PRIVATE 
    Arrow::arrow_shared 
    Parquet::parquet_shared
)

# Test executable
add_executable(run_tests
    tests/test_DataReader.cpp
    tests/test_OrderManager.cpp
    tests/test_Backtester.cpp
)

# Link libraries to test executable
target_link_libraries(run_tests PRIVATE 
    qse 
    Arrow::arrow_shared 
    Parquet::parquet_shared 
    gtest 
    gtest_main
)

# Add compile definition for test data directory
target_compile_definitions(run_tests PRIVATE 
    TEST_DATA_DIR="${TEST_DATA_DIR}"
)

# Add include directories for the run_tests target
target_include_directories(run_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Register tests
include(GoogleTest)
gtest_discover_tests(run_tests)

# Set CMAKE_PREFIX_PATH to include Homebrew directories
set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create the main application executable
add_executable(qse_app src/main.cpp)
target_link_libraries(qse_app PRIVATE qse Arrow::arrow_shared Parquet::parquet_shared) 