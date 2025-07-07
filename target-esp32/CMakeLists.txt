# ESP32 WSPR Beacon Project
cmake_minimum_required(VERSION 3.16)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Add component directories
set(EXTRA_COMPONENT_DIRS 
    "components"
    "../src/jtencode"
)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(wspr-beacon)