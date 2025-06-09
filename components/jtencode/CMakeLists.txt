# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.16)

# Conditionally include the correct build script based on the environment.
# This avoids parsing errors since CMake will not attempt to read files
# containing commands it doesn't understand for the current context.
if(DEFINED IDF_TARGET)
  # The IDF_TARGET variable is set by the ESP-IDF build system.
  include(component.cmake)
else()
  # If not building for ESP-IDF, assume a host build.
  include(host.cmake)
endif()

