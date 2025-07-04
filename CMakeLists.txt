# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(beacon)

# This builds either platform: target-esp32 or host-mock based on IDF_TARGET
#  environment variable.

# Sources common to both platforms.
add_subdirectory(src)

# ESP32 build
if(DEFINED ENV{IDF_TARGET})
  add_subdirectory(target-esp32)
else()
  add_subdirectory(host-mock)
endif()
