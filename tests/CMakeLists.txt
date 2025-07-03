cmake_minimum_required(VERSION 3.16)
project(WSPRBeaconTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(../include)
include_directories(../host-mock)
include_directories(../host-mock/external/json/include)

# Source files for the components being tested
set(SCHEDULER_SOURCES
    ../src/Scheduler.cpp
    ../src/FSM.cpp
)

# Mock implementations
set(MOCK_SOURCES
    ../host-mock/MockTimer.cpp
    ../host-mock/Settings.cpp
)

# Test executable
add_executable(test_runner
    TestRunner.cpp
    ${SCHEDULER_SOURCES}
    ${MOCK_SOURCES}
)

# Compiler flags
target_compile_options(test_runner PRIVATE -Wall -Wextra)

# Optional: Enable debug symbols for testing
set(CMAKE_BUILD_TYPE Debug)

# Custom target to run tests
add_custom_target(run_tests
    COMMAND test_runner
    DEPENDS test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running WSPR Beacon component tests"
)