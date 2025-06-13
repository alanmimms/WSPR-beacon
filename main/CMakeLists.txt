idf_component_register(
	SRCS "main.cpp"
	REQUIRES lwip esp_wifi nvs_flash jtencode si5351
	INCLUDE_DIRS "." "../include" "../components/jtencode/include" "../components/si5351/include")
