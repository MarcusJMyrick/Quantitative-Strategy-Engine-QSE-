cmake_minimum_required(VERSION 3.14)
project(QSE VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard and other initial settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build types" FORCE)

# ---
# FINAL FIX: Manually run pkg-config for all problematic libraries
# ---
find_package(PkgConfig REQUIRED)

# This block runs pkg-config and captures the compiler flags (-I...)
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags libzmq protobuf absl_log
    OUTPUT_VARIABLE PKG_CONFIG_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# This block runs pkg-config and captures the linker flags (-L... -l...)
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --libs libzmq protobuf absl_log
    OUTPUT_VARIABLE PKG_CONFIG_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Use standard find_package for libraries that are working correctly
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(Protobuf REQUIRED) # Still need this for the helper functions
find_package(yaml-cpp REQUIRED)


# --- Google Test Setup ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)


# --- Protocol Buffers Code Generation ---
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/messaging)
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})


# Add Homebrew yaml-cpp library path for linker
link_directories(/opt/homebrew/opt/yaml-cpp/lib)

# --- Main QSE Shared Library ---
add_library(qse SHARED
    src/core/Backtester.cpp
    src/core/Config.cpp
    src/core/ThreadPool.cpp
    src/data/BarBuilder.cpp
    src/data/CSVDataReader.cpp
    src/data/OrderBook.cpp
    src/data/ParquetDataReader.cpp
    src/data/ZeroMQDataReader.cpp
    src/messaging/TickPublisher.cpp
    src/messaging/TickSubscriber.cpp
    src/order/OrderManager.cpp
    src/strategy/MovingAverage.cpp
    src/strategy/MovingStandardDeviation.cpp
    src/strategy/SMACrossoverStrategy.cpp
    src/strategy/PairsTradingStrategy.cpp
    ${PROTO_SRCS}
)

# Link against the libraries discovered by pkg-config
target_link_libraries(qse PUBLIC
    Arrow::arrow_shared
    Parquet::parquet_shared
    yaml-cpp
    # Add the raw linker flags from our pkg-config command
    ${PKG_CONFIG_LIBS}
)

# Add the include directories discovered by pkg-config
target_include_directories(qse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    # Add the raw include flags from our pkg-config command
    ${PKG_CONFIG_CFLAGS}
)


# --- Executables ---
add_executable(run_tests
    tests/cpp/test_Messaging.cpp
    tests/cpp/test_DataReader.cpp
    tests/cpp/test_OrderManager.cpp
    tests/cpp/test_Backtester.cpp
    tests/cpp/test_Strategy.cpp
    tests/cpp/test_ThreadPool.cpp
    tests/cpp/test_BarBuilder.cpp
    tests/cpp/test_PairsTradingStrategy.cpp
    tests/cpp/test_CSVDataReaderTicks.cpp
    tests/cpp/test_Slippage.cpp
)

target_link_libraries(run_tests PRIVATE qse gmock gtest_main)

add_executable(messaging_example src/messaging_example.cpp)
target_link_libraries(messaging_example PRIVATE qse)

add_executable(data_publisher src/publisher/main.cpp)
target_link_libraries(data_publisher PRIVATE qse)

add_executable(strategy_engine src/engine/main.cpp)
target_link_libraries(strategy_engine PRIVATE qse)


# --- Testing Setup ---
enable_testing()
include(GoogleTest)
gtest_discover_tests(run_tests)