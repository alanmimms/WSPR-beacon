# CMakeLists.txt for the standalone jtencode test build
# This file is intended for building tests independent of the ESP-IDF environment.

# Specify the minimum required CMake version.
# Using a common version that supports modern CMake features.
cmake_minimum_required(VERSION 3.16)

# Define the project name for the standalone build.
project(JtEncodeTest CXX)

# Add the directory containing RSEncoder.h to include paths.
# Assuming RSEncoder.h is in the parent directory (components/jtencode).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Define the source files for the RSEncoder class.
# This assumes RSEncoder.cpp is in the parent directory.
set(RS_ENCODER_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../RSEncoder.cpp)

# Define your test source file.
# Assuming your main test file is named 'main.cpp' and is in the same directory as this CMakeLists.txt.
set(TEST_SRCS "main.cpp")

# Add the executable target for your test program.
add_executable(jtencode_test ${TEST_SRCS} ${RS_ENCODER_SRCS})

# Link any necessary libraries (e.g., standard C++ libraries, if not implicitly linked).
# For basic operations, often no explicit linking is needed here, but good to include if you add more.
# target_link_libraries(jtencode_test PRIVATE) # Add specific libs if needed
