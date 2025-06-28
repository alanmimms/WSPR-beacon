# CMakeLists.txt for consolidated jtencode test builds
# This file defines both the RSEncode class tests and the jtCode utility/component tests.

# Specify the minimum required CMake version.
cmake_minimum_required(VERSION 3.16)

# Define the project name for the consolidated test suite.
project(JtEncodeTests CXX)

# Add the parent directory (components/jtencode) to include paths,
# as it contains RSEncode.h, RSEncode.cpp, jtencode-util.h, and jtencode-util.cpp.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)


# --- Test Executable for RSEncode Class (test-rsencode) ---
# This executable specifically tests the RSEncode class functionality.

# Define the main source file for the RSEncode class test.
set(RS_ENCODE_TEST_MAIN_SRCS "rsencode-test-main.cpp")

# Define the source file(s) for the RSEncode class itself.
set(RS_ENCODE_CLASS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../RSEncode.cpp")

# Add the executable target.
add_executable(test-rsencode
  ${RS_ENCODE_TEST_MAIN_SRCS}
  ${RS_ENCODE_CLASS_SRCS}
)

# Link any necessary standard C++ libraries (optional, usually implicitly linked).
# target_link_libraries(test-rsencode PRIVATE)


# --- Test Executable for jtCode Utility / Overall Component (test-jtencode) ---
# This executable tests the jtCode utility and can implicitly test RSEncode
# if the jtencode-test-main.cpp uses it.

# Define the main source file for the jtCode/component test.
set(JT_ENCODE_TEST_MAIN_SRCS "jtencode-test-main.cpp")

# Define the source files for the jtCode utility.
# Changed from jtencode-util.cpp to jtencode-util.cpp
set(JT_CODE_UTIL_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../jtencode-util.cpp")

# Include RSEncode.cpp as a source for this executable, as you anticipate
# the comprehensive component test might implicitly use RSEncode.
set(JT_ENCODE_COMPONENT_EXTRA_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../RSEncode.cpp")


# Add the executable target.
add_executable(test-jtencode
  ${JT_ENCODE_TEST_MAIN_SRCS}
  ${JT_CODE_UTIL_SRCS}
  ${JT_ENCODE_COMPONENT_EXTRA_SRCS} # Include RSEncode.cpp here
)

# Link any necessary standard C++ libraries (optional, usually implicitly linked).
# target_link_libraries(test-jtencode PRIVATE)
