# This file contains the build logic for the host machine test executable.
# It is only included when IDF_TARGET is NOT defined.

project(jtencode_host CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(jtencode_lib STATIC JTEncode.cpp)
target_include_directories(jtencode_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Build the new generic test program named 'jtencode-test'
add_executable(jtencode-test test/jtencode-test.cpp)
target_link_libraries(jtencode-test PRIVATE jtencode_lib)

message(STATUS "Host build configured. Run 'make' to build the 'jtencode-test' executable.")
