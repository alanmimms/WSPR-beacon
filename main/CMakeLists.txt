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
        esp_wifi 
        esp_event
        nvs_flash 
        lwip 
	esp_driver_gpio
	esp_driver_i2c
        esp_http_server
	esp_timer
        mdns
        json

        # Local Project Components
        jtencode
	tinyfsm
        si5351
    INCLUDE_DIRS 
        "."
)
