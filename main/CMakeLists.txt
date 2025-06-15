idf_component_register(
    SRCS
        "main.cpp"
	"BeaconFSM.cpp"
        "ConfigManager.cpp"
        "WifiManager.cpp"
        "TimeManager.cpp"
        "WebServer.cpp"
        "Scheduler.cpp"
	"Settings.cpp"
    REQUIRES 
        # ESP-IDF Framework Components
	esp_driver_gpio
	esp_driver_i2c
	esp_netif
	esp_timer
	spiffs
        esp_event
        esp_http_server
        esp_wifi 
        json
        lwip 
        mdns
        nvs_flash 

        # Local Project Components
	dns-server
        jtencode
        si5351
	tinyfsm
    INCLUDE_DIRS 
        "."
)
