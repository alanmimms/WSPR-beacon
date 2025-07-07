#include "Net.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <string.h>
#include "dns-server.h"

static const char* TAG = "Net";
static esp_netif_t* ap_netif = nullptr;
static bool wifi_initialized = false;
static dns_server_handle_t dns_server = nullptr;

Net::Net() {}
Net::~Net() {}

bool Net::init() {
  if (wifi_initialized) {
    return true;
  }
  
  // Initialize WiFi
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t ret = esp_wifi_init(&wifi_init_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Set WiFi mode to AP
  ret = esp_wifi_set_mode(WIFI_MODE_AP);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Create AP netif
  ap_netif = esp_netif_create_default_wifi_ap();
  if (ap_netif == nullptr) {
    ESP_LOGE(TAG, "Failed to create AP netif");
    return false;
  }
  
  // Configure AP
  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.ap.ssid, "WSPR-Beacon");
  strcpy((char*)wifi_config.ap.password, "wspr1234");
  wifi_config.ap.ssid_len = strlen("WSPR-Beacon");
  wifi_config.ap.channel = 1;
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.ap.pmf_cfg.required = false;
  
  ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi set config failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Start WiFi
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  wifi_initialized = true;
  ESP_LOGI(TAG, "WiFi AP started: SSID='WSPR-Beacon', Password='wspr1234'");
  
  // Start DNS server for captive portal (redirect all domains to AP IP)
  dns_server_config_t dns_config = {
    .num_of_entries = 1,
    .item = { { .name = "*", .if_key = NULL, .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) } } }
  };
  dns_server = start_dns_server(&dns_config);
  if (dns_server) {
    ESP_LOGI(TAG, "DNS server started for captive portal");
  } else {
    ESP_LOGW(TAG, "Failed to start DNS server");
  }
  
  return true;
}

bool Net::connect(const char *ssid, const char *password) {
  ESP_LOGI(TAG, "Attempting to connect to WiFi SSID: %s", ssid);
  
  if (!ssid || strlen(ssid) == 0) {
    ESP_LOGE(TAG, "Invalid SSID provided");
    return false;
  }
  
  // Initialize WiFi in STA mode first
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
    return false;
  }
  
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
    return false;
  }
  
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  if (!sta_netif) {
    ESP_LOGE(TAG, "Failed to create STA netif");
    return false;
  }
  
  // Initialize WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Set WiFi mode to STA
  ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Configure STA
  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.sta.ssid, ssid);
  strcpy((char*)wifi_config.sta.password, password);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  
  ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi set config failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Start WiFi
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Connect to AP
  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  ESP_LOGI(TAG, "WiFi connection initiated, waiting for connection...");
  
  // Wait for connection (simple blocking wait for testing)
  for (int i = 0; i < 30; i++) {  // Wait up to 30 seconds
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      ESP_LOGI(TAG, "WiFi connected successfully to %s", ssid);
      wifi_initialized = true;
      
      // Get and log the IP address
      esp_netif_ip_info_t ip_info;
      esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected - IP address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Access web interface at: http://" IPSTR, IP2STR(&ip_info.ip));
      }
      
      return true;
    }
  }
  
  ESP_LOGE(TAG, "WiFi connection timeout");
  return false;
}

bool Net::disconnect() {
  if (wifi_initialized) {
    esp_wifi_stop();
    wifi_initialized = false;
  }
  return true;
}

bool Net::isConnected() {
  return wifi_initialized;
}

bool Net::startServer(int port) {
  // WiFi AP is started in init(), this just confirms it's running
  return wifi_initialized;
}

void Net::stopServer() {
  disconnect();
}

int Net::send(int clientId, const void *data, int len) {
  // Not implemented - HTTP server handles this
  return -1;
}

int Net::receive(int clientId, void *buffer, int maxLen) {
  // Not implemented - HTTP server handles this
  return -1;
}

void Net::closeClient(int clientId) {
  // Not implemented - HTTP server handles this
}

int Net::waitForClient() {
  // Not implemented - HTTP server handles this
  return -1;
}