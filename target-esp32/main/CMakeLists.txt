cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/target-esp32/components")

idf_component_register(
  SRCS
    ../../platform/esp32/main.cpp
    ../../platform/esp32/AppContext.cpp
    ../../platform/esp32/FileSystem.cpp
    ../../platform/esp32/GPIO.cpp
    ../../platform/esp32/Logger.cpp
    ../../platform/esp32/Net.cpp
    ../../platform/esp32/NVS.cpp
    ../../platform/esp32/Settings.cpp
    ../../platform/esp32/Si5351.cpp
    ../../platform/esp32/WebServer.cpp
    ../../platform/esp32/Timer.cpp
    ../../platform/esp32/Task.cpp
    ../../platform/esp32/EventGroup.cpp
    ../../platform/esp32/Time.cpp
    ../../platform/esp32/WSPRModulator.cpp
    ../../src/BeaconLogger.cpp
    ../../src/core/Beacon.cpp
    ../../src/core/FSM.cpp
    ../../src/core/Scheduler.cpp
  REQUIRES 
    # ESP-IDF Framework Components
    esp_http_server
    esp_wifi 
    nvs_flash 
    spiffs
    vfs
    driver
    lwip
    esp_netif
    esp_event

    # Local Project Components
    json
    dns-server
    jtencode
    si5351
  INCLUDE_DIRS 
    "."
    "../../include"
    "../../platform/esp32"
    "../../src"
)

# This is the command that handles the web assets.
# It finds the partition named "storage" in your partitions.csv,
# takes all the files from the "web" directory, and creates a
# binary SPIFFS image to be flashed to that partition.
spiffs_create_partition_image(storage ../../web FLASH_IN_PROJECT)
