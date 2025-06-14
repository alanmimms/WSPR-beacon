#include "WifiManager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mdns.h"
#include "lwip/inet.h"
#include <cstring>

static const char* TAG = "WifiManager";
// Global event group handle to signal FSM
extern EventGroupHandle_t app_event_group;
extern const int WIFI_PROV_TIMEOUT_BIT;
extern const int WIFI_CONNECTED_BIT;
extern const int WIFI_FAIL_BIT;

void WifiManager::init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, this, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void WifiManager::wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    static int s_retry_num = 0;
    const int MAX_RETRY = 10;

    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(app_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventData;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(app_event_group, WIFI_CONNECTED_BIT);
    }
}

void WifiManager::startProvisioningAP() {
    ESP_LOGI(TAG, "Starting Provisioning AP...");
    esp_wifi_stop(); // Ensure STA is stopped

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "Beacon-%02X%02X", mac[4], mac[5]);
    
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, ap_ssid);
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Provisioning AP started with SSID: %s", ap_ssid);
    startMdns(ap_ssid);
}

void WifiManager::startStationMode(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Starting Station Mode...");
    esp_wifi_stop(); // Ensure AP is stopped

    esp_netif_create_default_wifi_sta();
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WifiManager::stop() {
    esp_wifi_stop();
    mdns_free();
}

void WifiManager::startMdns(const char* hostname) {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("WSPR Beacon Controller"));
    // You can add services here if needed
}
