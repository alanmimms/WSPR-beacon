#include "Settings.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "Settings";
static const char* NVS_NAMESPACE = "beacon_cfg";

Settings::Settings() {
  memset(callsign, 0, sizeof(callsign));
  memset(locator, 0, sizeof(locator));
  powerDbm = 0;
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
  memset(hostname, 0, sizeof(hostname));
}

void Settings::load() {
  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  size_t requiredSize;

  // Load string values
  if (nvs_get_str(nvsHandle, "callsign", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "callsign", callsign, &requiredSize);
  }
  if (nvs_get_str(nvsHandle, "locator", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "locator", locator, &requiredSize);
  }
  if (nvs_get_str(nvsHandle, "ssid", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "ssid", ssid, &requiredSize);
  }
  if (nvs_get_str(nvsHandle, "password", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "password", password, &requiredSize);
  }
  if (nvs_get_str(nvsHandle, "hostname", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "hostname", hostname, &requiredSize);
  }

  // Load integer value
  nvs_get_i32(nvsHandle, "power_dbm", &powerDbm);
  
  nvs_close(nvsHandle);
  ESP_LOGI(TAG, "Settings loaded from NVS.");
}

void Settings::save() {
  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  nvs_set_str(nvsHandle, "callsign", callsign);
  nvs_set_str(nvsHandle, "locator", locator);
  nvs_set_i32(nvsHandle, "power_dbm", powerDbm);
  nvs_set_str(nvsHandle, "ssid", ssid);
  nvs_set_str(nvsHandle, "password", password);
  nvs_set_str(nvsHandle, "hostname", hostname);

  err = nvs_commit(nvsHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
  }
  
  nvs_close(nvsHandle);
  ESP_LOGI(TAG, "Settings saved to NVS.");
}

// --- Getters ---
const char* Settings::getCallsign() const { return callsign; }
const char* Settings::getLocator() const { return locator; }
int Settings::getPowerDbm() const { return powerDbm; }
const char* Settings::getSsid() const { return ssid; }
const char* Settings::getPassword() const { return password; }
const char* Settings::getHostname() const { return hostname; }


// --- Setters ---
void Settings::setCallsign(const char* newCallsign) { 
  strncpy(callsign, newCallsign, sizeof(callsign) - 1); 
}
void Settings::setLocator(const char* newLocator) { 
  strncpy(locator, newLocator, sizeof(locator) - 1); 
}
void Settings::setPowerDbm(int power) {
  powerDbm = power;
}
void Settings::setSsid(const char* newSsid) { 
  strncpy(ssid, newSsid, sizeof(ssid) - 1); 
}
void Settings::setPassword(const char* newPassword) { 
  strncpy(password, newPassword, sizeof(password) - 1); 
}
void Settings::setHostname(const char* newHostname) {
  strncpy(hostname, newHostname, sizeof(hostname) - 1);
}
