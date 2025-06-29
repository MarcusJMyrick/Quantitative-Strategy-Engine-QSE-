cmake_minimum_required(VERSION 3.14)
project(QSE VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard and other initial settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build types" FORCE)

# Find dependencies
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(Protobuf REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

# Find ZeroMQ using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

# Add Eigen as a subdirectory
add_subdirectory(third-party/eigen)

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


# --- Math Library (header-only) ---
add_library(qse_math INTERFACE)
target_include_directories(qse_math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
    src/strategy/FillTrackingStrategy.cpp
    src/strategy/MultiFactorStrategy.cpp
    src/factor/MultiFactorCalculator.cpp
    src/factor/UniverseFilter.cpp
    src/factor/CrossSectionalRegression.cpp
    src/factor/ICMonitor.cpp
    src/factor/AlphaBlender.cpp
    src/factor/RiskModel.cpp
    src/factor/PortfolioBuilder.cpp
    src/exe/FactorExecutionEngine.cpp
    ${PROTO_SRCS}
)

# Add the include directories
target_include_directories(qse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Arrow_INCLUDE_DIRS}
    ${Parquet_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
    ${ZeroMQ_INCLUDE_DIRS}
)

# Link the library to its dependencies
target_link_libraries(qse PUBLIC
    Arrow::arrow_shared
    Parquet::parquet_shared
    yaml-cpp::yaml-cpp
    Eigen3::Eigen
    Threads::Threads
    ${Protobuf_LIBRARIES}
    GTest::gtest
    GTest::gmock
    ${ZeroMQ_LIBRARIES}
)

# Add ZeroMQ library directories
target_link_directories(qse PUBLIC ${ZeroMQ_LIBRARY_DIRS})

# --- Executables ---
add_executable(run_tests
    tests/cpp/test_Messaging.cpp
    tests/cpp/test_DataReader.cpp
    tests/cpp/test_OrderManager.cpp
    tests/cpp/test_Backtester.cpp
    tests/cpp/test_Strategy.cpp
    tests/cpp/test_ThreadPool.cpp
    tests/cpp/test_BarBuilder.cpp
    tests/cpp/test_CSVDataReaderTicks.cpp
    tests/cpp/test_Slippage.cpp
    tests/cpp/test_LimitOrderFill.cpp
    tests/cpp/test_TickDrivenArchitecture.cpp
    tests/cpp/test_FillCallback.cpp
    tests/cpp/test_SmokeTest.cpp
    tests/cpp/test_BarDrivenStrategies.cpp
    tests/cpp/test_BarDrivenIntegration.cpp
    tests/cpp/test_CSVTimestampUnits.cpp
    tests/cpp/CrossSectionalRegressionTest.cpp
    tests/cpp/ICMonitorTest.cpp
    tests/cpp/AlphaBlenderTest.cpp
    tests/cpp/BetaCalcTest.cpp
    tests/cpp/AlphaBlenderICIntegrationTest.cpp
    tests/cpp/RiskModelMultiAssetTest.cpp
    tests/cpp/AlphaBlenderWeightPropertyTest.cpp
    tests/cpp/UniverseFuzzTest.cpp
    tests/cpp/CrossSectionalRegressionRobustTest.cpp
    tests/cpp/ICMonitorDistributionTest.cpp
    tests/cpp/PortfolioBuilderTest.cpp
    tests/cpp/WeightsLoadTest.cpp
    tests/cpp/HoldingsSnapshotTest.cpp
    tests/cpp/DiffCalcTest.cpp
)

target_link_libraries(run_tests PRIVATE qse gmock gtest_main)

# --- Factor Calculator Tests ---
add_executable(FactorMathTest tests/cpp/FactorMathTest.cpp)
target_link_libraries(FactorMathTest gtest gtest_main qse_math)

add_executable(FactorCalcTest tests/cpp/FactorCalcTest.cpp)
target_link_libraries(FactorCalcTest gtest gtest_main yaml-cpp::yaml-cpp)
target_include_directories(FactorCalcTest PRIVATE /opt/homebrew/include)

add_executable(UniverseFilterTest tests/cpp/UniverseFilterTest.cpp)
target_link_libraries(UniverseFilterTest gtest gtest_main qse)

# --- Factor Calculator Tool ---
add_executable(compute_factors src/tools/compute_factors.cpp)
target_link_libraries(compute_factors PRIVATE qse)

add_executable(messaging_example src/messaging_example.cpp)
target_link_libraries(messaging_example PRIVATE qse)

add_executable(data_publisher src/publisher/main.cpp)
target_link_libraries(data_publisher PRIVATE qse)

add_executable(strategy_engine src/engine/main.cpp)
target_link_libraries(strategy_engine PRIVATE qse)

add_executable(multi_symbol_engine src/engine/multi_symbol_main.cpp)
target_link_libraries(multi_symbol_engine PRIVATE qse)

add_executable(multi_strategy_engine src/engine/multi_strategy_main.cpp)
target_link_libraries(multi_strategy_engine PRIVATE qse)

add_executable(direct_test scripts/testing/direct_test.cpp)
target_link_libraries(direct_test PRIVATE qse)

add_executable(factor_exec_demo src/tools/factor_exec_demo.cpp)
target_link_libraries(factor_exec_demo qse)

# --- Testing Setup ---
enable_testing()
include(GoogleTest)
gtest_discover_tests(run_tests)

# Add factor calculator tests
add_test(NAME FactorMathTest COMMAND FactorMathTest)
add_test(NAME FactorCalcTest COMMAND FactorCalcTest)
add_test(NAME UniverseFilterTest COMMAND UniverseFilterTest)