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

# Find ZeroMQ library - try multiple methods
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZEROMQ QUIET libzmq)
endif()

if(ZEROMQ_FOUND)
    message(STATUS "Found ZeroMQ via pkg-config")
    # pkg-config provides ZEROMQ_LIBRARIES and ZEROMQ_INCLUDE_DIRS
else()
    # Try to find ZeroMQ manually
    find_library(ZEROMQ_LIBRARIES
        NAMES zmq libzmq
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    find_path(ZEROMQ_INCLUDE_DIRS
        NAMES zmq.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    
    if(ZEROMQ_LIBRARIES AND ZEROMQ_INCLUDE_DIRS)
        set(ZEROMQ_FOUND TRUE)
        message(STATUS "Found ZeroMQ manually: ${ZEROMQ_LIBRARIES}")
    else()
        message(WARNING "ZeroMQ not found - distributed system components will be disabled")
        set(ZEROMQ_FOUND FALSE)
    endif()
endif()

# Find cppzmq headers
find_path(CPPZMQ_INCLUDE_DIRS
    NAMES zmq.hpp
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)

if(CPPZMQ_INCLUDE_DIRS)
    message(STATUS "Found cppzmq headers: ${CPPZMQ_INCLUDE_DIRS}")
    set(CPPZMQ_FOUND TRUE)
else()
    message(WARNING "cppzmq headers not found - distributed system components will be disabled")
    set(CPPZMQ_FOUND FALSE)
endif()

# Only enable ZeroMQ features if both library and headers are found
if(ZEROMQ_FOUND AND CPPZMQ_FOUND)
    set(ZEROMQ_ENABLED TRUE)
    message(STATUS "ZeroMQ and cppzmq found - enabling distributed system components")
    
    # Create imported target for ZeroMQ if not already created
    if(NOT TARGET ZeroMQ::zmq)
        add_library(ZeroMQ::zmq UNKNOWN IMPORTED)
        
        # Set the library location
        if(ZEROMQ_LIBRARIES)
            set_target_properties(ZeroMQ::zmq PROPERTIES
                IMPORTED_LOCATION "${ZEROMQ_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${ZEROMQ_INCLUDE_DIRS};${CPPZMQ_INCLUDE_DIRS}"
            )
        else()
            # For pkg-config, we need to find the actual library file
            find_library(ZMQ_LIBRARY
                NAMES zmq libzmq
                PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
            )
            if(ZMQ_LIBRARY)
                set_target_properties(ZeroMQ::zmq PROPERTIES
                    IMPORTED_LOCATION "${ZMQ_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${ZEROMQ_INCLUDE_DIRS};${CPPZMQ_INCLUDE_DIRS}"
                )
            else()
                message(ERROR "Could not find ZeroMQ library file")
                set(ZEROMQ_ENABLED FALSE)
            endif()
        endif()
        message(STATUS "Created ZeroMQ::zmq imported target")
    endif()
else()
    set(ZEROMQ_ENABLED FALSE)
    message(STATUS "ZeroMQ or cppzmq not found - disabling distributed system components")
endif()

# Find Protocol Buffers library and tools
find_package(Protobuf QUIET)
if(NOT Protobuf_FOUND)
    # Try to find protobuf manually
    find_library(PROTOBUF_LIBRARIES
        NAMES protobuf libprotobuf
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    find_path(PROTOBUF_INCLUDE_DIRS
        NAMES google/protobuf/message.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    find_program(PROTOC_EXECUTABLE
        NAMES protoc
        PATHS /opt/homebrew/bin /usr/local/bin /usr/bin
    )
    
    if(PROTOBUF_LIBRARIES AND PROTOBUF_INCLUDE_DIRS AND PROTOC_EXECUTABLE)
        set(Protobuf_FOUND TRUE)
        set(protobuf::libprotobuf INTERFACE IMPORTED)
        set_target_properties(protobuf::libprotobuf PROPERTIES
            INTERFACE_LINK_LIBRARIES "${PROTOBUF_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDE_DIRS}"
        )
        message(STATUS "Found Protocol Buffers manually: ${PROTOBUF_LIBRARIES}")
    else()
        message(WARNING "Protocol Buffers not found - distributed system components will be disabled")
        set(Protobuf_FOUND FALSE)
    endif()
endif()

# Set the directory where the .proto file is
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/messaging)

# Find the .proto file
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")

# Tell Protobuf to generate C++ files from the .proto file.
# The generated files will be placed in the build directory.
if(Protobuf_FOUND AND PROTO_FILES)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
    message(STATUS "Protocol Buffers files will be generated")
else()
    set(PROTO_SRCS "")
    set(PROTO_HDRS "")
    message(STATUS "Protocol Buffers generation disabled")
endif()

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
set(QSE_SOURCES
    src/core/Backtester.cpp
    src/core/ThreadPool.cpp
    src/data/BarBuilder.cpp
    src/data/CSVDataReader.cpp
    src/data/ParquetDataReader.cpp
    src/order/OrderManager.cpp
    src/strategy/MovingAverage.cpp
    src/strategy/SMACrossoverStrategy.cpp
)

# Add distributed system components if dependencies are available
if(ZEROMQ_ENABLED)
    list(APPEND QSE_SOURCES
        src/data/ZeroMQDataReader.cpp
        src/messaging/TickPublisher.cpp
        src/messaging/TickSubscriber.cpp
    )
    message(STATUS "Adding messaging components to QSE library")
endif()

if(Protobuf_FOUND)
    list(APPEND QSE_SOURCES ${PROTO_SRCS})
    message(STATUS "Adding Protocol Buffers generated files to QSE library")
endif()

add_library(qse SHARED ${QSE_SOURCES})

# Link libraries to qse
target_link_libraries(qse PRIVATE 
    Arrow::arrow_shared 
    Parquet::parquet_shared
)

# Add ZeroMQ if found
if(ZEROMQ_ENABLED)
    target_link_libraries(qse PUBLIC ZeroMQ::zmq)
    message(STATUS "Linked ZeroMQ to QSE library using imported target")
endif()

# Add Protocol Buffers if found
if(Protobuf_FOUND)
    target_link_libraries(qse PRIVATE protobuf::libprotobuf)
    message(STATUS "Linked Protocol Buffers to QSE library")
endif()

# Include directories for qse - updated to use new structure
target_include_directories(qse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add generated protobuf headers if available
if(Protobuf_FOUND)
    target_include_directories(qse PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()

# --- Test executable ---
# Adding test files with updated paths
set(TEST_SOURCES
    tests/cpp/test_DataReader.cpp
    tests/cpp/test_OrderManager.cpp
    tests/cpp/test_Backtester.cpp
    tests/cpp/test_Strategy.cpp
    tests/cpp/test_ThreadPool.cpp
    tests/cpp/test_BarBuilder.cpp
    tests/cpp/test_Messaging.cpp
)

# Add ZeroMQ tests if available
if(ZEROMQ_ENABLED)
    list(APPEND TEST_SOURCES tests/cpp/test_ZeroMQDataReader.cpp)
    message(STATUS "Adding ZeroMQ tests to test suite")
endif()

add_executable(run_tests ${TEST_SOURCES})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/mocks
)

# Add generated protobuf headers if available
if(Protobuf_FOUND)
    target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

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

# Add generated protobuf headers if available
if(Protobuf_FOUND)
    target_include_directories(qse_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

# --- Create the messaging example executable ---
if(ZEROMQ_ENABLED)
    add_executable(messaging_example src/messaging_example.cpp)
    target_link_libraries(messaging_example PRIVATE qse)
    target_include_directories(messaging_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if(Protobuf_FOUND)
        target_include_directories(messaging_example PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    message(STATUS "Created messaging_example executable")
endif()

# --- Create the data publisher executable ---
if(ZEROMQ_ENABLED AND Protobuf_FOUND)
    add_executable(data_publisher src/publisher/main.cpp)
    target_link_libraries(data_publisher PRIVATE qse)
    target_link_libraries(data_publisher PRIVATE protobuf::libprotobuf)
    target_include_directories(data_publisher PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    message(STATUS "Created data_publisher executable")
endif()

# --- Create the strategy engine executable ---
if(ZEROMQ_ENABLED AND Protobuf_FOUND)
    add_executable(strategy_engine src/engine/main.cpp)
    target_link_libraries(strategy_engine PRIVATE qse)
    target_link_libraries(strategy_engine PRIVATE protobuf::libprotobuf)
    target_include_directories(strategy_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    message(STATUS "Created strategy_engine executable")
endif()