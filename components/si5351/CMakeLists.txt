idf_component_register(
	SRCS "si5351.cpp"
	# Corrected the component name from esp_driver_i2c to driver
	PRIV_REQUIRES driver
	INCLUDE_DIRS "include")
