cmake_minimum_required(VERSION 3.10)
project(QSE VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# Add source files
set(SOURCES
    src/DataReader.cpp
)

# Add header files
set(HEADERS
    src/Data.h
    src/DataReader.h
)

# Create library
add_library(qse SHARED ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(qse
    PRIVATE
    Arrow::arrow_shared
    Parquet::parquet_shared
)

# Include directories
target_include_directories(qse
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Install rules
install(TARGETS qse
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES ${HEADERS} DESTINATION include/qse)

# ===================================================================
# Testing Setup (Google Test)
# ===================================================================
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# For macOS/Linux, to avoid re-downloading every time:
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

# Define your test executable
add_executable(run_tests tests/test_DataReader.cpp)

# Link your main library and Google Test to the test executable
target_link_libraries(run_tests 
    PRIVATE 
    qse 
    Arrow::arrow_shared
    Parquet::parquet_shared
    gtest_main
)

# Add the test to CTest so it can be run with `ctest`
include(GoogleTest)
gtest_discover_tests(run_tests) 