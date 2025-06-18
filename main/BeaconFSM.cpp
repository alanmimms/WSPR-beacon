#include "BeaconFSM.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// --- Debugging Flag ---
// Set to 1 to use static Wi-Fi credentials from secrets.h for debugging.
// Set to 0 for normal operation (AP provisioning).
#define USE_STATIC_WIFI_CREDS 1

#if USE_STATIC_WIFI_CREDS
#include "secrets.h"
#endif

static const char* TAG = "BeaconFSM";

static const int MAX_WIFI_CONNECT_ATTEMPTS = 10;
static const int PROVISIONING_TIMEOUT_MINUTES = 5;

// State forward declarations
class InitialState;
class ConnectingState;
class ProvisioningState;
class ConnectedState;
class TransmittingState;

// State class definitions
class InitialState : public BeaconState { public: using BeaconState::BeaconState; void enter() override; };
class ConnectingState : public BeaconState { public: using BeaconState::BeaconState; void enter() override; };
class ProvisioningState : public BeaconState { public: using BeaconState::BeaconState; void enter() override; void exit() override; };
class ConnectedState : public BeaconState { public: using BeaconState::BeaconState; void enter() override; };
class TransmittingState : public BeaconState { public: using BeaconState::BeaconState; void enter() override; };

// BeaconFsm implementation
BeaconFsm::BeaconFsm() :
  si5351(CONFIG_SI5351_ADDRESS, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL),
  currentState(nullptr),
  settings(),
  webServer(),
  scheduler(si5351, settings),
  wifiRetryTimer(nullptr),
  provisionTimer(nullptr),
  wifiConnectAttempts(0)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &BeaconFsm::onWifiStaDisconnected, this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &BeaconFsm::onWifiStaGotIp, this));

  settings.load();
  
  const esp_timer_create_args_t provisionTimerArgs = {.callback = &BeaconFsm::onProvisionTimeout, .arg = this, .name = "provision-timeout"};
  ESP_ERROR_CHECK(esp_timer_create(&provisionTimerArgs, &provisionTimer));

  const esp_timer_create_args_t wifiRetryTimerArgs = {.callback = &BeaconFsm::onWifiRetryTimeout, .arg = this, .name = "wifi-retry"};
  ESP_ERROR_CHECK(esp_timer_create(&wifiRetryTimerArgs, &wifiRetryTimer));
}

BeaconFsm::~BeaconFsm() {
  delete currentState;
}

void BeaconFsm::start() {
  transitionTo(new InitialState(this));
}

void BeaconFsm::transitionTo(BeaconState* newState) {
  if (currentState != nullptr) {
    currentState->exit();
    delete currentState;
  }
  currentState = newState;
  if (currentState != nullptr) {
    currentState->enter();
  }
}

void BeaconFsm::onProvisionTimeout(void* arg) { static_cast<BeaconFsm*>(arg)->provisionTimeout(); }
void BeaconFsm::onWifiRetryTimeout(void* arg) { static_cast<BeaconFsm*>(arg)->wifiRetryTimeout(); }
void BeaconFsm::onWifiStaGotIp(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) { static_cast<BeaconFsm*>(arg)->wifiStaGotIp(eventData); }
void BeaconFsm::onWifiStaDisconnected(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) { static_cast<BeaconFsm*>(arg)->wifiStaDisconnected(eventData); }

void BeaconFsm::provisionTimeout() {
  ESP_LOGI(TAG, "Provisioning timed out. Retrying Wi-Fi connection.");
  transitionTo(new ConnectingState(this));
}

void BeaconFsm::wifiRetryTimeout() {
  ESP_LOGI(TAG, "Wi-Fi retry timeout expired. Retrying connection.");
  transitionTo(new ConnectingState(this));
}

void BeaconFsm::wifiStaGotIp(void* eventData) {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventData;
  ESP_LOGI(TAG, "***************************************************");
  ESP_LOGI(TAG, "Beacon connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
  ESP_LOGI(TAG, "***************************************************");
  transitionTo(new ConnectedState(this));
}

void BeaconFsm::wifiStaDisconnected(void* eventData) {
  ESP_LOGW(TAG, "Wi-Fi disconnected.");
  
  #if USE_STATIC_WIFI_CREDS
    ESP_LOGI(TAG, "Debug mode: Retrying connection to static AP...");
    transitionTo(new ConnectingState(this));
  #else
    wifiConnectAttempts++;
    if (wifiConnectAttempts >= MAX_WIFI_CONNECT_ATTEMPTS) {
      ESP_LOGE(TAG, "Failed to connect after %d attempts. Entering provisioning mode.", MAX_WIFI_CONNECT_ATTEMPTS);
      wifiConnectAttempts = 0;
      transitionTo(new ProvisioningState(this));
    } else {
      esp_timer_start_once(wifiRetryTimer, 5 * 1000 * 1000);
    }
  #endif
}

void InitialState::enter() {
  #if USE_STATIC_WIFI_CREDS
    ESP_LOGI(TAG, "Debug mode enabled. Using static credentials from secrets.h");
    // Temporarily overwrite settings with static credentials for connection attempt.
    context->settings.setValue("ssid", WIFI_SSID);
    context->settings.setValue("password", WIFI_PASSWORD);
    context->transitionTo(new ConnectingState(context));
  #else
    ESP_LOGI(TAG, "Entering Initial state");
    const char* ssid = context->settings.getValue("ssid");
    if (ssid && strlen(ssid) > 0) {
      context->transitionTo(new ConnectingState(context));
    } else {
      ESP_LOGI(TAG, "No SSID configured, entering provisioning mode.");
      context->transitionTo(new ProvisioningState(context));
    }
  #endif
}

void ConnectingState::enter() {
  ESP_LOGI(TAG, "Connecting to Wi-Fi...");
  context->connectToWifi();
}

void ProvisioningState::enter() {
  ESP_LOGI(TAG, "Entering Provisioning state for %d minutes.", PROVISIONING_TIMEOUT_MINUTES);
  context->startProvisioningMode();
  esp_timer_start_once(context->provisionTimer, (uint64_t)PROVISIONING_TIMEOUT_MINUTES * 60 * 1000000);
}

void ProvisioningState::exit() {
  ESP_LOGI(TAG, "Exiting Provisioning state");
  esp_timer_stop(context->provisionTimer);
  context->webServer.stop();
}

void ConnectedState::enter() {
  ESP_LOGI(TAG, "Entering Connected state.");
  context->wifiConnectAttempts = 0;
  context->startNtpSync();
  context->transitionTo(new TransmittingState(context));
  context->webServer.start(context->settings);
}

void TransmittingState::enter() {
  ESP_LOGI(TAG, "Entering Transmitting state. System is operational.");
  // Start the transmission scheduler
  context->scheduler.start();
}

void BeaconFsm::connectToWifi() {
  wifi_config_t wifiConfig = {};
  strncpy((char*)wifiConfig.sta.ssid, settings.getValue("ssid"), sizeof(wifiConfig.sta.ssid) - 1);
  strncpy((char*)wifiConfig.sta.password, settings.getValue("password"), sizeof(wifiConfig.sta.password) - 1);
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Connection request sent for SSID: %s", settings.getValue("ssid"));
  ESP_ERROR_CHECK(esp_wifi_connect());
}

void BeaconFsm::startProvisioningMode() {
  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
  
  wifi_config_t wifi_config = {};
  snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "Beacon-%02X%02X", mac[4], mac[5]);
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  wifi_config.ap.max_connection = 4;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_netif_ip_info_t ip_info;
  ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info));
  
  ESP_LOGI(TAG, "Provisioning AP '%s' started.", wifi_config.ap.ssid);
  ESP_LOGI(TAG, "Connect and go to http://" IPSTR, IP2STR(&ip_info.ip));
  context->webServer.start(context->settings);
}

void BeaconFsm::startNtpSync() {
  ESP_LOGI(TAG, "Starting NTP synchronization...");
}
