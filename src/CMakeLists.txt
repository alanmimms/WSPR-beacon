add_library(beacon_core STATIC
  core/Beacon.cpp
  core/FSM.cpp
  core/Scheduler.cpp
)

target_include_directories(beacon_core PUBLIC 
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${CMAKE_CURRENT_LIST_DIR}/jtencode/include
  ${CMAKE_CURRENT_LIST_DIR}/../external/cjson
)

target_link_libraries(beacon_core PUBLIC cjson)
