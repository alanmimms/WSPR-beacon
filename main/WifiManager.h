#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_event.h"

class WifiManager {
public:
    void init();
    void startProvisioningAP();
    void startStationMode(const char* ssid, const char* password);
    void stop();

    void startMdns(const char* hostname);
private:
    static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
};

#endif // WIFI_MANAGER_H
