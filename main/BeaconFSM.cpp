#include "BeaconFSM.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include <string.h>

// Required for CONFIG_ macros
#include "sdkconfig.h"

// Required ESP-IDF driver and utility headers
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_mac.h" // Required for esp_efuse_mac_get_default

static const char* TAG = "BeaconFSM";

// --- Constants ---
static const int MAX_WIFI_CONNECT_ATTEMPTS = 10;
static const int PROVISIONING_TIMEOUT_MINUTES = 5;

// --- Forward declarations of our concrete state classes ---
class InitialState;
class ConnectingState;
class ProvisioningState;
class ConnectedState;
class TransmittingState;

// --- State Class Implementations ---

class InitialState : public BeaconState {
public:
  using BeaconState::BeaconState; // Inherit constructor
  void enter() override;
};

class ConnectingState : public BeaconState {
public:
  using BeaconState::BeaconState;
  void enter() override;
};

class ProvisioningState : public BeaconState {
public:
  using BeaconState::BeaconState;
  void enter() override;
  void exit() override;
};

class ConnectedState : public BeaconState {
public:
  using BeaconState::BeaconState;
  void enter() override;
};

class TransmittingState : public BeaconState {
public:
  using BeaconState::BeaconState;
  void enter() override;
};


// --- BeaconFsm Method Implementations ---

BeaconFsm::BeaconFsm() :
  // The order here MUST match the declaration order in BeaconFSM.h
  si5351(CONFIG_SI5351_ADDRESS, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL),
  currentState(nullptr),
  settings(),
  webServer(),
  scheduler(si5351, settings),
  wifiRetryTimer(nullptr),
  provisionTimer(nullptr),
  wifiConnectAttempts(0)
{
  // --- Perform all one-time system initializations first ---
  
  // 1. Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 2. Initialize Networking Stack (Netif, Event Loop, WiFi)
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // --- Now it is safe to register event handlers ---
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &BeaconFsm::onWifiStaDisconnected, this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &BeaconFsm::onWifiStaGotIp, this));

  // --- Load settings and create other components ---
  settings.load();
  
  const esp_timer_create_args_t provisionTimerArgs = {
      .callback = &BeaconFsm::onProvisionTimeout,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "provision-timeout",
      .skip_unhandled_events = false
  };
  ESP_ERROR_CHECK(esp_timer_create(&provisionTimerArgs, &provisionTimer));

  const esp_timer_create_args_t wifiRetryTimerArgs = {
      .callback = &BeaconFsm::onWifiRetryTimeout,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "wifi-retry",
      .skip_unhandled_events = false
  };
  ESP_ERROR_CHECK(esp_timer_create(&wifiRetryTimerArgs, &wifiRetryTimer));
}

BeaconFsm::~BeaconFsm() {
  // Clean up the current state if it exists
  delete currentState;
}

void BeaconFsm::start() {
  // Create and transition to the initial state to kick things off.
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

// --- Static Callback Wrappers ---

void BeaconFsm::onProvisionTimeout(void* arg) { static_cast<BeaconFsm*>(arg)->provisionTimeout(); }
void BeaconFsm::onWifiRetryTimeout(void* arg) { static_cast<BeaconFsm*>(arg)->wifiRetryTimeout(); }
void BeaconFsm::onWifiStaGotIp(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) { static_cast<BeaconFsm*>(arg)->wifiStaGotIp(eventData); }
void BeaconFsm::onWifiStaDisconnected(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) { static_cast<BeaconFsm*>(arg)->wifiStaDisconnected(eventData); }

// --- FSM Handler Methods ---

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
  ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
  transitionTo(new ConnectedState(this));
}

void BeaconFsm::wifiStaDisconnected(void* eventData) {
  ESP_LOGW(TAG, "Wi-Fi disconnected.");
  wifiConnectAttempts++;
  if (wifiConnectAttempts >= MAX_WIFI_CONNECT_ATTEMPTS) {
    ESP_LOGE(TAG, "Failed to connect after %d attempts. Entering provisioning mode.", MAX_WIFI_CONNECT_ATTEMPTS);
    wifiConnectAttempts = 0; // Reset for the next cycle
    transitionTo(new ProvisioningState(this));
  } else {
    esp_timer_start_once(wifiRetryTimer, 5 * 1000 * 1000); 
  }
}

// --- Concrete State Method Implementations ---

void InitialState::enter() {
  ESP_LOGI(TAG, "Entering Initial state");
  if (strlen(context->settings.getSsid()) > 0) {
    context->transitionTo(new ConnectingState(context));
  } else {
    ESP_LOGI(TAG, "No SSID configured, entering provisioning mode.");
    context->transitionTo(new ProvisioningState(context));
  }
}

void ConnectingState::enter() {
  ESP_LOGI(TAG, "Entering Connecting state, attempt %d/%d", context->wifiConnectAttempts + 1, MAX_WIFI_CONNECT_ATTEMPTS);
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
  context->stopWebServer();
}

void ConnectedState::enter() {
  ESP_LOGI(TAG, "Entering Connected state.");
  context->wifiConnectAttempts = 0;
  context->startNtpSync();
  context->transitionTo(new TransmittingState(context));
}

void TransmittingState::enter() {
  ESP_LOGI(TAG, "Entering Transmitting state. System is operational.");
  // Now that the system is connected, we can start the scheduler.
  context->scheduler.start();
}

// --- General Instance Method Implementations ---

void BeaconFsm::connectToWifi() {
  wifi_config_t wifiConfig = {};
  strncpy((char*)wifiConfig.sta.ssid, settings.getSsid(), sizeof(wifiConfig.sta.ssid) - 1);
  strncpy((char*)wifiConfig.sta.password, settings.getPassword(), sizeof(wifiConfig.sta.password) - 1);
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  ESP_LOGI(TAG, "Connection request sent for SSID: %s", settings.getSsid());
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

  webServer.start(settings);
}

void BeaconFsm::stopWebServer() {
  ESP_LOGI(TAG, "Stopping web server and Wi-Fi.");
  webServer.stop();
  esp_wifi_stop();
}

void BeaconFsm::startNtpSync() {
  ESP_LOGI(TAG, "Starting NTP synchronization...");
  // NTP client initialization and sync logic goes here
}
