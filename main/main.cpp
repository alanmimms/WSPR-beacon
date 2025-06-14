#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "tinyfsm.hpp"

#include "ConfigManager.h"
#include "WifiManager.h"
#include "TimeManager.h"
#include "WebServer.h"
#include "Scheduler.h"

static const char* TAG = "MAIN";

// --- Event Group for cross-task communication ---
EventGroupHandle_t app_event_group;
const int WIFI_PROV_TIMEOUT_BIT = BIT0;
const int WIFI_CONNECTED_BIT = BIT1;
const int WIFI_FAIL_BIT = BIT2;

// --- FSM Event Declarations ---
struct WifiCheck : tinyfsm::Event {};
struct ProvisioningTimeout : tinyfsm::Event {};
struct WifiConnectFailed : tinyfsm::Event {};
struct WifiConnected : tinyfsm::Event {};

// --- FSM State Declarations & FSM Definition ---
class BeaconFsm; // Forward declaration
struct InitState;
struct CheckWiFiState;
struct ProvisioningState;
struct ConnectingState;
struct ConnectedState;
struct FailureState;

class BeaconFsm : public tinyfsm::Fsm<BeaconFsm> {
public:
    // Default react definitions
    virtual void react(WifiCheck const &) { ESP_LOGI(TAG, "FSM: Ignoring WifiCheck event"); }
    virtual void react(ProvisioningTimeout const &) { ESP_LOGI(TAG, "FSM: Ignoring ProvisioningTimeout event"); }
    virtual void react(WifiConnectFailed const &) { ESP_LOGI(TAG, "FSM: Ignoring WifiConnectFailed event"); }
    virtual void react(WifiConnected const &) { ESP_LOGI(TAG, "FSM: Ignoring WifiConnected event"); }
    
    virtual void entry(void) = 0;
    void exit(void) {}

    // Managers for hardware and services
    static WifiManager wifiManager;
    static WebServer webServer;
    static TimeManager timeManager;
    static Scheduler scheduler;
};
WifiManager BeaconFsm::wifiManager;
WebServer BeaconFsm::webServer;
TimeManager BeaconFsm::timeManager;
Scheduler BeaconFsm::scheduler;


// --- State Implementations ---

struct InitState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering InitState");
        // Initialize NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        // Initialize WiFi subsystem
        wifiManager.init();

        // Move to the next state to check credentials
        transit<CheckWiFiState>();
    }
};

struct CheckWiFiState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering CheckWiFiState");
        BeaconConfig config;
        ConfigManager::loadConfig(config);
        if (strlen(config.wifiSsid) > 0) {
            ESP_LOGI(TAG, "Found Wi-Fi credentials for SSID: %s. Trying to connect.", config.wifiSsid);
            transit<ConnectingState>();
        } else {
            ESP_LOGI(TAG, "No Wi-Fi credentials found. Starting provisioning.");
            transit<ProvisioningState>();
        }
    }
};

struct ProvisioningState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering ProvisioningState");
        wifiManager.startProvisioningAP();
        webServer.start(); // Start web server for configuration
        xEventGroupSetBits(app_event_group, WIFI_PROV_TIMEOUT_BIT);
    }
    void react(ProvisioningTimeout const &) override {
        ESP_LOGI(TAG, "FSM: Provisioning timeout. Retrying connection to saved network.");
        webServer.stop();
        transit<CheckWiFiState>();
    }
    void exit() override {
        webServer.stop();
    }
};

struct ConnectingState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering ConnectingState");
        BeaconConfig config;
        ConfigManager::loadConfig(config);
        wifiManager.startStationMode(config.wifiSsid, config.wifiPassword);
    }
    void react(WifiConnected const &) override {
        ESP_LOGI(TAG, "FSM: Reacting to WifiConnected event.");
        transit<ConnectedState>();
    }
    void react(WifiConnectFailed const &) override {
        ESP_LOGI(TAG, "FSM: Reacting to WifiConnectFailed event.");
        transit<FailureState>();
    }
};

struct ConnectedState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering ConnectedState");
        BeaconConfig config;
        ConfigManager::loadConfig(config);
        
        // Start network services
        wifiManager.startMdns(strlen(config.hostname) > 0 ? config.hostname : "wspr-beacon");
        timeManager.init(config.timeZone);
        webServer.start(); // For control and status pages

        // Start main application tasks
        xTaskCreate([](void* p){((Scheduler*)p)->run();}, "Scheduler", 8192, &scheduler, 5, NULL);
        xTaskCreate([](void* p){((TimeManager*)p)->runAdaptiveSync();}, "TimeManager", 4096, &timeManager, 5, NULL);
    }
};

struct FailureState : BeaconFsm {
    void entry() override {
        ESP_LOGI(TAG, "FSM: Entering FailureState. Wi-Fi connection failed after 10 retries.");
        ESP_LOGI(TAG, "Starting provisioning AP for 5 minutes.");
        // This task will wait 5 minutes then signal to restart the process
        xTaskCreate([](void*){
            vTaskDelay(pdMS_TO_TICKS(5 * 60 * 1000));
            BeaconFsm::dispatch(ProvisioningTimeout());
        }, "FailureTimer", 2048, NULL, 5, NULL);
        
        transit<ProvisioningState>();
    }
};

// Set initial state
FSM_INITIAL_STATE(BeaconFsm, InitState)

// --- Main Application Entry Point ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting WSPR Beacon Application");
    app_event_group = xEventGroupCreate();

    // Initialize and start the state machine
    BeaconFsm::start();
    
    // Main loop to drive the FSM based on events
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(app_event_group, 
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, 
            pdTRUE, // Clear bits on exit
            pdFALSE, // Don't wait for all bits
            portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            BeaconFsm::dispatch(WifiConnected());
        } else if (bits & WIFI_FAIL_BIT) {
            BeaconFsm::dispatch(WifiConnectFailed());
        }
    }
}
