cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/target/components")

idf_component_register(
  SRCS
    main.cpp
    AppContext.cpp
    FileSystem.cpp
    GPIO.cpp
    Logger.cpp
    Net.cpp
    NVS.cpp
    Settings.cpp
    Si5351.cpp
    WebServer.cpp
    Timer.cpp
    Task.cpp
    EventGroup.cpp
  REQUIRES 
    cxx
    # ESP-IDF Framework Components
    esp_driver_gpio
    esp_driver_i2c
    esp_netif
    esp_system
    esp_timer
    esp_event
    esp_http_server
    esp_wifi 
    json
    lwip 
    mbedtls
    mdns
    nvs_flash 
    spiffs

    # Local Project Components
    dns-server
    jtencode
    si5351
  INCLUDE_DIRS 
    "."
    "../include"
)

# This is the command that handles the web assets.
# It finds the partition named "storage" in your partitions.csv,
# takes all the files from the "web" directory, and creates a
# binary SPIFFS image to be flashed to that partition.
spiffs_create_partition_image(storage ../web FLASH_IN_PROJECT)
