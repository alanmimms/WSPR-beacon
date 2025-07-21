# WSPR Beacon Project Build Configuration
cmake_minimum_required(VERSION 3.16)

# Detect if we're being called from the wrong directory
get_filename_component(CURRENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
if(NOT CURRENT_DIR STREQUAL "WSPR-beacon")
    message(FATAL_ERROR 
        "\n"
        "========================================\n"
        "❌ BUILD ERROR: Wrong Directory!\n"
        "========================================\n"
        "\n"
        "You're trying to build from: ${CMAKE_CURRENT_SOURCE_DIR}\n"
        "\n"
        "For ESP32 builds, you should run:\n"
        "  cd target-esp32\n"
        "  idf.py build\n"
        "\n"
        "For host-mock builds, you should run:\n"
        "  cd host-mock\n"
        "  mkdir build && cd build\n"
        "  cmake .. && make\n"
        "\n"
        "Or use the smart build script:\n"
        "  ./build.sh esp32        # Build ESP32\n"
        "  ./build.sh host         # Build host-mock\n"
        "  ./build.sh both         # Build both\n"
        "\n"
        "See ./build.sh --help for more options.\n"
        "========================================\n"
    )
endif()

# Check if this is an ESP-IDF build (should only happen in target-esp32/)
if(DEFINED ENV{IDF_PATH} AND DEFINED ENV{IDF_TARGET})
    message(FATAL_ERROR 
        "\n"
        "========================================\n"
        "❌ ESP-IDF BUILD ERROR!\n"
        "========================================\n"
        "\n"
        "ESP-IDF environment detected, but you're in the wrong directory.\n"
        "\n"
        "For ESP32 builds, run:\n"
        "  cd target-esp32\n"
        "  idf.py build\n"
        "\n"
        "Current directory: ${CMAKE_CURRENT_SOURCE_DIR}\n"
        "Expected directory: ${CMAKE_CURRENT_SOURCE_DIR}/target-esp32\n"
        "\n"
        "Or use: ./build.sh esp32\n"
        "========================================\n"
    )
endif()

# This top-level CMakeLists.txt is only for documentation and error detection.
# Actual builds should use the platform-specific directories:
#   - target-esp32/   (for ESP32 hardware)
#   - host-mock/      (for host testing)

message(STATUS "WSPR Beacon Project Structure:")
message(STATUS "  📁 target-esp32/  - ESP32 hardware build")
message(STATUS "  📁 host-mock/     - Host testing build") 
message(STATUS "  📁 src/           - Common source code")
message(STATUS "  📁 include/       - Common headers")
message(STATUS "  📁 platform/      - Platform implementations")
message(STATUS "")
message(STATUS "To build:")
message(STATUS "  ESP32:     cd target-esp32 && idf.py build")
message(STATUS "  Host:      cd host-mock && mkdir build && cd build && cmake .. && make")
message(STATUS "  Smart:     ./build.sh [esp32|host|both] [build|clean|flash|test]")

# Don't actually build anything from here - just provide guidance
message(FATAL_ERROR "Please use platform-specific build directories or ./build.sh script")
