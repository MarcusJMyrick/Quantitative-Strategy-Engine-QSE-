cmake_minimum_required(VERSION 3.10)
project(QuantitativeStrategyEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    src/main.cpp
    src/DataReader.cpp
    src/MovingAverageCrossStrategy.cpp
    src/OrderManager.cpp
)

# Add header files
set(HEADERS
    src/Data.h
    src/DataReader.h
    src/IStrategy.h
    src/MovingAverageCrossStrategy.h
    src/OrderManager.h
)

# Create executable
add_executable(qse ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(qse PRIVATE ${CMAKE_SOURCE_DIR}/src) 