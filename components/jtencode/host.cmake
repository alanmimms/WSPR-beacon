# This file contains the build logic for the host machine test executable.
# It is only included when IDF_TARGET is NOT defined.

# Define the project and enable C++
project(jtencode_host CXX)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create a static library from our encoder source files.
add_library(jtencode_lib STATIC JTEncode.cpp)

# Make the library's public headers available to whatever links against it.
target_include_directories(jtencode_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add the executable for our host-based test program.
add_executable(test-wspr test/test-wspr.cpp)

# Link the test program against our encoder library.
target_link_libraries(test-wspr PRIVATE jtencode_lib)

# Print a message to the user confirming the build mode.
message(STATUS "Host build configured. Run 'make' to build the 'test-wspr' executable.")
