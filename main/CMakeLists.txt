idf_component_register(
    SRCS
        "main.cpp"
        "ConfigManager.cpp"
        "WifiManager.cpp"
        "TimeManager.cpp"
        "WebServer.cpp"
        "Scheduler.cpp"
    REQUIRES 
        # ESP-IDF Framework Components
        esp_wifi 
        esp_event
        nvs_flash 
        lwip 
        esp_http_server
        mdns
        json

        # Local Project Components
        jtencode
	tinyfsm
        si5351
    INCLUDE_DIRS 
        "."
)
