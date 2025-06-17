idf_component_register(
    SRCS
        "main.cpp"
        "BeaconFSM.cpp"
        "Settings.cpp"
        "ConfigManager.cpp"
        "Scheduler.cpp"
        "TimeManager.cpp"
        "WebServer.cpp"
        "WifiManager.cpp"
    REQUIRES 
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
        mdns
        nvs_flash 
        spiffs

        # Local Project Components
        dns-server
        jtencode
        si5351
        tinyfsm
    INCLUDE_DIRS 
        "."
)
