#include "BeaconFSM.h"
#include "Settings.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "si5351.h"
#include "Scheduler.h"
#include "WebServer.h"

#define BYPASS_PROVISIONING

#ifdef BYPASS_PROVISIONING
#include "secrets.h"
#endif

static const char *TAG = "BeaconFSM";

static EventGroupHandle_t wifiEventGroup;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

#define STATUS_LED_GPIO static_cast<gpio_num_t>(CONFIG_STATUS_LED_GPIO)


static const char *defaultSettingsJson =
  "{"
    "\"callsign\":\"N0CALL\","
    "\"locator\":\"AA00aa\","
    "\"powerDbm\":10,"
    "\"txIntervalMinutes\":4"
  "}";


BeaconFSM::BeaconFSM()
  : currentState(State::BOOTING),
    webServer(nullptr),
    si5351(nullptr),
    scheduler(nullptr),
    settings(nullptr) {}

BeaconFSM::~BeaconFSM() {
  delete scheduler;
  delete si5351;
  delete webServer;
  delete settings;
}

void BeaconFSM::wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
  if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "WiFi disconnected.");
    if (wifiEventGroup) {
      xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
    }
  } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) eventData;
    ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    if (wifiEventGroup) {
      xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    }
  } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STACONNECTED) {
    ESP_LOGI(TAG, "Station connected to AP");
  }
}

void BeaconFSM::initHardware() {
  ESP_LOGI(TAG, "constructing and loading Settings");
  settings = new Settings(defaultSettingsJson);

  gpio_reset_pin(STATUS_LED_GPIO);
  gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
  ESP_LOGI(TAG, "GPIO driver initialized for status LED.");
}

void BeaconFSM::run() {
  while (true) {
    switch (currentState) {
      case State::BOOTING:        handleBooting();        break;
      case State::AP_MODE:        handleApMode();         break;
      case State::STA_CONNECTING: handleStaConnecting();  break;
      case State::BEACONING:      handleBeaconing();      break;
      case State::ERROR:
        ESP_LOGE(TAG, "Entering error state. Halting.");
        vTaskDelay(portMAX_DELAY);
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void BeaconFSM::handleBooting() {
  ESP_LOGI(TAG, "State: BOOTING");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  static const esp_vfs_spiffs_conf_t spiffsConf = {
    .base_path = WebServer::spiffsBasePath,
    .partition_label = "storage",
    .max_files = 10,
    .format_if_mount_failed = false,
  };
  ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffsConf));

  initHardware();

  webServer = new WebServer(settings);
  si5351 = new Si5351(CONFIG_SI5351_ADDRESS, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL, CONFIG_I2C_MASTER_FREQUENCY);
  scheduler = new Scheduler(si5351, settings, STATUS_LED_GPIO);

#ifdef BYPASS_PROVISIONING
  ESP_LOGW(TAG, "Bypassing provisioning, connecting with credentials from secrets.h");
  currentState = State::STA_CONNECTING;
#else
  char ssid[MAX_SSID_LEN];
  settings->getString("ssid", ssid, sizeof(ssid), "");
  if (strlen(ssid) > 0) {
    currentState = State::STA_CONNECTING;
  } else {
    currentState = State::AP_MODE;
  }
#endif
}

void BeaconFSM::handleApMode() {
  ESP_LOGI(TAG, "State: AP_MODE");
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, this));

  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifiConfig = {};
  strncpy((char *) wifiConfig.ap.ssid, "JTEncode Beacon", sizeof(wifiConfig.ap.ssid) - 1);
  wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
  wifiConfig.ap.max_connection = 4;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "SoftAP Started.");

  webServer->start();
  currentState = State::BEACONING;
}

void BeaconFSM::handleStaConnecting() {
  ESP_LOGI(TAG, "State: STA_CONNECTING");
  wifiEventGroup = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, this));
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifiConfig = {};
#ifdef BYPASS_PROVISIONING
  strncpy((char *) wifiConfig.sta.ssid, WIFI_SSID, sizeof(wifiConfig.sta.ssid) - 1);
  strncpy((char *) wifiConfig.sta.password, WIFI_PASSWORD, sizeof(wifiConfig.sta.password) - 1);
#else
  char ssid[MAX_SSID_LEN];
  char password[MAX_PASSWORD_LEN];
  settings->getString("ssid", ssid, sizeof(ssid), "");
  settings->getString("password", password, sizeof(password), "");
  strncpy((char *) wifiConfig.sta.ssid, ssid, sizeof(wifiConfig.sta.ssid) - 1);
  strncpy((char *) wifiConfig.sta.password, password, sizeof(wifiConfig.sta.password) - 1);
#endif

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for WiFi connection...");

  EventBits_t bits = xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Connected to AP.");
    webServer->start();
    currentState = State::BEACONING;
  } else {
    ESP_LOGW(TAG, "Failed to connect to AP. Falling back to AP Mode.");
    if (webServer) webServer->stop();
    esp_wifi_stop();
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler);
    esp_netif_t *wifiNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifiNetif) {
      esp_netif_destroy(wifiNetif);
    }
    currentState = State::AP_MODE;
  }
  vEventGroupDelete(wifiEventGroup);
  wifiEventGroup = NULL;
}

void BeaconFSM::handleBeaconing() {
  ESP_LOGI(TAG, "State: BEACONING. Starting scheduler.");
  if (scheduler) {
    scheduler->start();
  }

  while (true) {
    vTaskDelay(portMAX_DELAY);
  }
}
