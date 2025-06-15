#ifndef APP_GLOBALS_H
#define APP_GLOBALS_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Declare the event group handle as an external variable.
// Any .cpp file that includes this header can access appEventGroup.
extern EventGroupHandle_t appEventGroup;

// Declare the bit definitions as external constants.
extern const int WIFI_PROV_TIMEOUT_BIT;
extern const int WIFI_CONNECTED_BIT;
extern const int WIFI_FAIL_BIT;

#endif // APP_GLOBALS_H
