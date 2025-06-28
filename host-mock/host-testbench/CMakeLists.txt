cmake_minimum_required(VERSION 3.16)

project(wspr-beacon-host-test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add dependencies as subdirectories for ease of use in-tree
# Assumes that you have added nlohmann/json and cpp-httplib as submodules or downloaded them into these paths:
#   main/external/json (for nlohmann/json)
#   main/external/cpp-httplib (for cpp-httplib)

# nlohmann/json
add_subdirectory(external/json)

# cpp-httplib is header-only, just add its include directory
include_directories(${CMAKE_CURRENT_LIST_DIR}/external/cpp-httplib)

add_executable(host-testbench
  main.cpp
  test-server.cpp
  test-server.h
  # Add other test files as needed
)

target_include_directories(host-testbench PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/..
  ${CMAKE_CURRENT_LIST_DIR}/external/cpp-httplib
)

# Link nlohmann_json target
target_link_libraries(host-testbench PRIVATE nlohmann_json::nlohmann_json)


# Link OpenSSL for cpp-httplib if SSL support is enabled
find_package(OpenSSL REQUIRED)
target_link_libraries(host-testbench PRIVATE OpenSSL::SSL OpenSSL::Crypto)
