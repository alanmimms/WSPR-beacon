#include "WifiManager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "mdns.h"
#include "lwip/inet.h"
#include <cstring>

// Include the new header for global declarations
#include "app-globals.h"

static const char* TAG = "WifiManager";

void WifiManager::init() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, this, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, this, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void WifiManager::wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  static int retryNum = 0;
  const int MAX_RETRY = 10;

  if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
    if (retryNum < MAX_RETRY) {
      esp_wifi_connect();
      retryNum++;
      ESP_LOGI(TAG, "Retry to connect to the AP");
    } else {
      xEventGroupSetBits(appEventGroup, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "Connect to the AP fail");
  } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventData;
    ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    retryNum = 0;
    xEventGroupSetBits(appEventGroup, WIFI_CONNECTED_BIT);
  }
}

void WifiManager::startProvisioningAP() {
  ESP_LOGI(TAG, "Starting Provisioning AP...");
  esp_wifi_stop();

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  char apSsid[32];
  snprintf(apSsid, sizeof(apSsid), "Beacon-%02X%02X", mac[4], mac[5]);
    
  wifi_config_t wifiConfig = {};
  strcpy((char*)wifiConfig.ap.ssid, apSsid);
  wifiConfig.ap.ssid_len = strlen(apSsid);
  wifiConfig.ap.channel = 1;
  wifiConfig.ap.max_connection = 4;
  wifiConfig.ap.authmode = WIFI_AUTH_OPEN;

  esp_netif_create_default_wifi_ap();
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Provisioning AP started with SSID: %s", apSsid);
  startMdns(apSsid);
}

void WifiManager::startStationMode(const char* ssid, const char* password) {
  ESP_LOGI(TAG, "Starting Station Mode...");
  esp_wifi_stop();

  esp_netif_create_default_wifi_sta();
  wifi_config_t wifiConfig = {};
  strcpy((char*)wifiConfig.sta.ssid, ssid);
  strcpy((char*)wifiConfig.sta.password, password);
    
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
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
}
