add_library(beacon_core STATIC
  BeaconFSM.cpp
)

target_include_directories(beacon_core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)
