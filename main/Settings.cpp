#include "Settings.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "Settings";
static const char* NVS_NAMESPACE = "beacon_cfg";

Settings::Settings() {
  // Initialize with empty C strings to ensure they are null-terminated
  memset(callsign, 0, sizeof(callsign));
  memset(locator, 0, sizeof(locator));
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
}

void Settings::load() {
  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  size_t requiredSize;

  // Load Callsign
  if (nvs_get_str(nvsHandle, "callsign", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "callsign", callsign, &requiredSize);
  }
  // Load Locator
  if (nvs_get_str(nvsHandle, "locator", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "locator", locator, &requiredSize);
  }
  // Load SSID
  if (nvs_get_str(nvsHandle, "ssid", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "ssid", ssid, &requiredSize);
  }
  // Load Password
  if (nvs_get_str(nvsHandle, "password", NULL, &requiredSize) == ESP_OK) {
    nvs_get_str(nvsHandle, "password", password, &requiredSize);
  }
  
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
  nvs_set_str(nvsHandle, "ssid", ssid);
  nvs_set_str(nvsHandle, "password", password);

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
const char* Settings::getSsid() const { return ssid; }
const char* Settings::getPassword() const { return password; }

// --- Setters ---
void Settings::setCallsign(const char* newCallsign) { 
  strncpy(callsign, newCallsign, MAX_CALLSIGN_LEN - 1); 
}
void Settings::setLocator(const char* newLocator) { 
  strncpy(locator, newLocator, MAX_LOCATOR_LEN - 1); 
}
void Settings::setSsid(const char* newSsid) { 
  strncpy(ssid, newSsid, MAX_SSID_LEN - 1); 
}
void Settings::setPassword(const char* newPassword) { 
  strncpy(password, newPassword, MAX_PASSWORD_LEN - 1); 
}
