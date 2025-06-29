#include "BeaconFSM.h"
#include "Settings.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "si5351.h"
#include "WebServer.h"
#include "esp_sntp.h"
#include <time.h>

#define BYPASS_PROVISIONING

#ifdef BYPASS_PROVISIONING
#include "secrets.h"
#endif

static const char *TAG = "BeaconFSM";

static EventGroupHandle_t wifiEventGroup;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

#define STATUS_LED_GPIO static_cast<gpio_num_t>(CONFIG_STATUS_LED_GPIO)
#define STATUS_LED_OFF 1
#define STATUS_LED_ON  (!STATUS_LED_OFF)

static const char *defaultSettingsJson =
  "{"
    "\"callsign\":\"N0CALL\","
    "\"locator\":\"AA00aa\","
    "\"powerDbm\":10,"
    "\"txIntervalMinutes\":4"
  "}";

BeaconFSM::BeaconFSM(AppContext *ctx)
  : currentState(State::BOOTING),
    webServer(nullptr),
    settings(nullptr),
    txDurationTimer(nullptr),
    txScheduleTimer(nullptr),
    txCanceled(false),
    txInProgress(false),
    ctx(ctx) {}

BeaconFSM::~BeaconFSM() {
  if (txDurationTimer) esp_timer_delete(txDurationTimer);
  if (txScheduleTimer) esp_timer_delete(txScheduleTimer);
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

  if (ctx && ctx->gpio) {
    ctx->gpio->init();
    ctx->gpio->setOutput(CONFIG_STATUS_LED_GPIO, STATUS_LED_OFF);
  } else {
    gpio_reset_pin(STATUS_LED_GPIO);
    gpio_set_level(STATUS_LED_GPIO, STATUS_LED_OFF);
    gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
  }
  ESP_LOGI(TAG, "GPIO driver initialized for status LED.");
}

void BeaconFSM::run() {
  while (true) {
    switch (currentState) {
      case State::BOOTING:        handleBooting();        break;
      case State::AP_MODE:        handleApMode();         break;
      case State::STA_CONNECTING: handleStaConnecting();  break;
      case State::BEACONING:      handleBeaconing();      break;
      case State::TX_START:       handleTxStart();        break;
      case State::TX_END:         handleTxEnd();          break;
      case State::ERROR:
        ESP_LOGE(TAG, "Entering error state. Halting.");
        vTaskDelay(portMAX_DELAY);
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
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

  webServer = new WebServer(settings, ctx);
  webServer->setFsmCallback([this]() { this->onSettingsChanged(); });

  // Si5351 is now from context, not allocated here
  // ctx->si5351 should be set up in main/platform code

  // Set up SNTP
  syncTime();

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
  ESP_LOGI(TAG, "State: BEACONING. Scheduling next transmit if allowed.");
  txCanceled = false;
  txInProgress = false;
  scheduleNextTransmit();
  while (currentState == State::BEACONING) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void BeaconFSM::handleTxStart() {
  ESP_LOGI(TAG, "State: TX_START");
  txCanceled = false;
  txInProgress = true;
  startTransmit();
  // Duration is fixed WSPR slot (e.g., 110 seconds)
  const int txDurationSec = 110;
  esp_timer_stop(txDurationTimer); // Stop if running
  if (!txDurationTimer) {
    const esp_timer_create_args_t timerArgs = {
      .callback = &BeaconFSM::txDurationTimerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "tx_duration_timer",
      .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &txDurationTimer));
  }
  ESP_ERROR_CHECK(esp_timer_start_once(txDurationTimer, txDurationSec * 1000000ULL));
  // Wait for timer or cancel
  while (currentState == State::TX_START) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void BeaconFSM::handleTxEnd() {
  ESP_LOGI(TAG, "State: TX_END");
  endTransmit();
  txInProgress = false;
  scheduleNextTransmit();
  currentState = State::BEACONING;
}

void BeaconFSM::startTransmit() {
  ESP_LOGI(TAG, "TX cycle: %s, %s, %ddBm.", settings->getString("callsign"), settings->getString("locator"), settings->getInt("powerDbm", 10));
  if (ctx && ctx->gpio) {
    ctx->gpio->setOutput(CONFIG_STATUS_LED_GPIO, STATUS_LED_ON);
  } else {
    gpio_set_level(STATUS_LED_GPIO, STATUS_LED_ON);
  }
  // TODO: Actual transmit logic using ctx->si5351 etc.
}

void BeaconFSM::endTransmit() {
  ESP_LOGI(TAG, "TX end (normal/cancel).");
  if (ctx && ctx->gpio) {
    ctx->gpio->setOutput(CONFIG_STATUS_LED_GPIO, STATUS_LED_OFF);
  } else {
    gpio_set_level(STATUS_LED_GPIO, STATUS_LED_OFF);
  }
  // TODO: Clean up TX hardware if needed
}

void BeaconFSM::scheduleNextTransmit() {
  // Schedule next TX at next WSPR boundary (even minute, etc.)
  time_t now = time(NULL);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  int secondsPastMinute = tmNow.tm_sec;
  int minutesPastHour = tmNow.tm_min % settings->getInt("txIntervalMinutes", 4);
  int secondsToNextTx = ((settings->getInt("txIntervalMinutes", 4) - minutesPastHour) * 60) - secondsPastMinute;
  if (secondsToNextTx < 0) secondsToNextTx += settings->getInt("txIntervalMinutes", 4) * 60;
  if (secondsToNextTx < 5) secondsToNextTx += settings->getInt("txIntervalMinutes", 4) * 60; // Never schedule too close

  ESP_LOGI(TAG, "Next transmit in %d seconds.", secondsToNextTx);

  if (!txScheduleTimer) {
    const esp_timer_create_args_t timerArgs = {
      .callback = &BeaconFSM::txScheduleTimerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "tx_sched_timer",
      .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &txScheduleTimer));
  }
  esp_timer_stop(txScheduleTimer);
  ESP_ERROR_CHECK(esp_timer_start_once(txScheduleTimer, (uint64_t)secondsToNextTx * 1000000ULL));
}

void BeaconFSM::txDurationTimerCallback(void *arg) {
  BeaconFSM *fsm = static_cast<BeaconFSM *>(arg);
  if (!fsm->txCanceled) {
    fsm->currentState = State::TX_END;
  }
}

void BeaconFSM::txScheduleTimerCallback(void *arg) {
  BeaconFSM *fsm = static_cast<BeaconFSM *>(arg);
  if (!fsm->txInProgress && fsm->canTransmitNow()) {
    fsm->currentState = State::TX_START;
  }
}

void BeaconFSM::cancelTransmit() {
  if (txInProgress) {
    txCanceled = true;
    if (txDurationTimer) esp_timer_stop(txDurationTimer);
    currentState = State::TX_END;
  }
}

void BeaconFSM::onSettingsChanged() {
  ESP_LOGI(TAG, "Settings changed via WebServer.");
  cancelTransmit();
  if (canTransmitNow()) {
    currentState = State::BEACONING;
  }
}

bool BeaconFSM::canTransmitNow() {
  // For WSPR, TX must start at start of even minute, interval, etc.
  time_t now = time(NULL);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  int interval = settings->getInt("txIntervalMinutes", 4);
  if ((tmNow.tm_min % interval) == 0 && tmNow.tm_sec < 2) {
    return true;
  }
  return false;
}

// SNTP/time sync stubs

void BeaconFSM::syncTime() {
  ESP_LOGI(TAG, "Starting SNTP time sync...");
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();
}

void BeaconFSM::periodicTimeSync() {
  static time_t lastSync = 0;
  time_t now = time(NULL);
  if ((now - lastSync) > 3600) { // sync every hour
    syncTime();
    lastSync = now;
  }
}