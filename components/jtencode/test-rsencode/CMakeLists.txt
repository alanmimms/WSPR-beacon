# CMakeLists.txt for the standalone RSEncode test build
# This file is intended for building tests independent of the ESP-IDF environment.

# Specify the minimum required CMake version.
cmake_minimum_required(VERSION 3.16)

# Define the project name for this test executable.
project(JtEncodeRSEncodeTest CXX) # Changed project name

# Add the directory containing RSEncode.h and RSEncode.cpp to include paths.
# They are located in the parent directory (components/jtencode).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Define the source files for the RSEncode class.
# This assumes RSEncode.cpp is in the parent directory.
set(RS_ENCODE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../RSEncode.cpp) # Changed variable name and file path

# Define the source file for this test executable.
set(TEST_MAIN_SRCS "main.cpp")

# Add the executable target for the RSEncode test program.
add_executable(jtencode-rsencode-test ${TEST_MAIN_SRCS} ${RS_ENCODE_SRCS}) # Changed executable name and source variable

# Link any necessary standard C++ libraries.
# Usually, these are implicitly linked, but it's good practice to be explicit if needed.
# target_link_libraries(jtencode-rsencode-test PRIVATE) # Add specific libs if needed
