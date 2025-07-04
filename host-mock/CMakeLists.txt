cmake_minimum_required(VERSION 3.16)

project(wspr-beacon-host-test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add dependencies as subdirectories for ease of use in-tree
# Assumes that you have added nlohmann/json and cpp-httplib as submodules or downloaded them into these paths:
#   main/external/json (for nlohmann/json)
#   main/external/cpp-httplib (for cpp-httplib)

# Add the shared src directory as a library
add_subdirectory(../src ${CMAKE_BINARY_DIR}/src)

# nlohmann/json
add_subdirectory(external/json)

# cpp-httplib is header-only, just add its include directory
include_directories(${CMAKE_CURRENT_LIST_DIR}/external/cpp-httplib)

add_executable(host-testbench
  ../platform/host-mock/main.cpp
  ../platform/host-mock/test-server.cpp
  ../platform/host-mock/AppContext.cpp
  ../platform/host-mock/FileSystem.cpp
  ../platform/host-mock/GPIO.cpp
  ../platform/host-mock/Logger.cpp
  ../platform/host-mock/Net.cpp
  ../platform/host-mock/NVS.cpp
  ../platform/host-mock/Settings.cpp
  ../platform/host-mock/Si5351.cpp
  ../platform/host-mock/WebServer.cpp
  ../platform/host-mock/Timer.cpp
  ../platform/host-mock/Task.cpp
  ../platform/host-mock/EventGroup.cpp
  ../platform/host-mock/Time.cpp
)

target_include_directories(host-testbench PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${CMAKE_CURRENT_LIST_DIR}/../src
  ${CMAKE_CURRENT_LIST_DIR}/../platform/host-mock
  ${CMAKE_CURRENT_LIST_DIR}/external/cpp-httplib
)

target_link_libraries(host-testbench PRIVATE beacon_core)

# Link nlohmann_json target
target_link_libraries(host-testbench PRIVATE nlohmann_json::nlohmann_json)

# Link OpenSSL for cpp-httplib if SSL support is enabled
find_package(OpenSSL REQUIRED)
target_link_libraries(host-testbench PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Add test target
add_custom_target(tests
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/host-testbench
  DEPENDS host-testbench
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running web server tests"
)
