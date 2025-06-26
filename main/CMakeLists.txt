cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

project("wspr-beacon")

# ESP32 build
if(DEFINED ENV{IDF_TARGET})
  set(SOURCES
        "main.cpp"
        "BeaconFSM.cpp"
        "Settings.cpp"
        "WebServer.cpp")

  idf_component_register(
    SRCS ${SOURCES}
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
  )
else()
  # Host-based test bench
  add_subdirectory(host_testbench)
endif()
