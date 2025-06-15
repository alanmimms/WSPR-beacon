#include "ConfigManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h" // For serializing complex data
#include <string.h>

static const char* NVS_NAMESPACE = "beacon_config";
static const char* TAG = "ConfigManager";

// Helper to serialize and save a JSON string to NVS
static bool saveJsonToNvs(nvs_handle_t handle, const char* key, cJSON* json) {
  char* jsonString = cJSON_PrintUnformatted(json);
  if (jsonString == NULL) {
    ESP_LOGE(TAG, "Failed to print JSON to string.");
    return false;
  }
  esp_err_t err = nvs_set_str(handle, key, jsonString);
  free(jsonString);
  return err == ESP_OK;
}

// Helper to load and parse a JSON string from NVS
static cJSON* loadJsonFromNvs(nvs_handle_t handle, const char* key) {
  size_t required_size = 0;
  esp_err_t err = nvs_get_str(handle, key, NULL, &required_size);
  if (err != ESP_OK || required_size == 0) {
    return NULL;
  }
  char* jsonString = (char*)malloc(required_size);
  if (jsonString == NULL) {
    return NULL;
  }
  err = nvs_get_str(handle, key, jsonString, &required_size);
  if (err != ESP_OK) {
    free(jsonString);
    return NULL;
  }
  cJSON* json = cJSON_Parse(jsonString);
  free(jsonString);
  return json;
}

bool ConfigManager::loadConfig(BeaconConfig& config) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;

  // Load simple types
  size_t len;
  nvs_get_str(handle, "wifi_ssid", config.wifiSsid, &(len = sizeof(config.wifiSsid)));
  nvs_get_str(handle, "wifi_pass", config.wifiPassword, &(len = sizeof(config.wifiPassword)));
  nvs_get_str(handle, "hostname", config.hostname, &(len = sizeof(config.hostname)));
  nvs_get_str(handle, "callsign", config.callsign, &(len = sizeof(config.callsign)));
  nvs_get_str(handle, "locator", config.locator, &(len = sizeof(config.locator)));
  nvs_get_i8(handle, "power_dbm", &config.powerDbm);
  nvs_get_u8(handle, "is_running", (uint8_t*)&config.isRunning);
  nvs_get_i32(handle, "skip_int", (int32_t*)&config.skipIntervals);
  nvs_get_str(handle, "time_zone", config.timeZone, &(len = sizeof(config.timeZone)));

  // Load complex types (schedules, band plan) from JSON
  cJSON* schedules_json = loadJsonFromNvs(handle, "schedules");
  if (schedules_json && cJSON_IsArray(schedules_json)) {
    int i = 0;
    cJSON* item;
    cJSON_ArrayForEach(item, schedules_json) {
      if (i >= 5) break;
      config.timeSchedules[i].enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "en"));
      config.timeSchedules[i].startHour = cJSON_GetObjectItem(item, "sh")->valueint;
      config.timeSchedules[i].startMinute = cJSON_GetObjectItem(item, "sm")->valueint;
      config.timeSchedules[i].endHour = cJSON_GetObjectItem(item, "eh")->valueint;
      config.timeSchedules[i].endMinute = cJSON_GetObjectItem(item, "em")->valueint;
      config.timeSchedules[i].daysOfWeek = cJSON_GetObjectItem(item, "d")->valueint;
      i++;
    }
  }
  if (schedules_json) cJSON_Delete(schedules_json);

  cJSON* bands_json = loadJsonFromNvs(handle, "bands");
  if (bands_json && cJSON_IsArray(bands_json)) {
    int i = 0;
    cJSON* item;
    config.numBandsInPlan = cJSON_GetArraySize(bands_json);
    cJSON_ArrayForEach(item, bands_json) {
      if (i >= 5) break;
      config.bandPlan[i].frequencyHz = cJSON_GetObjectItem(item, "f")->valuedouble; // cJSON uses double for all numbers
      config.bandPlan[i].iterations = cJSON_GetObjectItem(item, "i")->valueint;
      i++;
    }
  } else {
    config.numBandsInPlan = 0;
  }
  if(bands_json) cJSON_Delete(bands_json);

  nvs_close(handle);
  return true;
}

bool ConfigManager::saveConfig(const BeaconConfig& config) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;

  // Save simple types
  nvs_set_str(handle, "wifi_ssid", config.wifiSsid);
  nvs_set_str(handle, "wifi_pass", config.wifiPassword);
  nvs_set_str(handle, "hostname", config.hostname);
  nvs_set_str(handle, "callsign", config.callsign);
  nvs_set_str(handle, "locator", config.locator);
  nvs_set_i8(handle, "power_dbm", config.powerDbm);
  nvs_set_u8(handle, "is_running", config.isRunning);
  nvs_set_i32(handle, "skip_int", config.skipIntervals);
  nvs_set_str(handle, "time_zone", config.timeZone);

  // Create JSON for schedules
  cJSON *schedules_json = cJSON_CreateArray();
  for (int i = 0; i < 5; ++i) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddBoolToObject(item, "en", config.timeSchedules[i].enabled);
    cJSON_AddNumberToObject(item, "sh", config.timeSchedules[i].startHour);
    cJSON_AddNumberToObject(item, "sm", config.timeSchedules[i].startMinute);
    cJSON_AddNumberToObject(item, "eh", config.timeSchedules[i].endHour);
    cJSON_AddNumberToObject(item, "em", config.timeSchedules[i].endMinute);
    cJSON_AddNumberToObject(item, "d", config.timeSchedules[i].daysOfWeek);
    cJSON_AddItemToArray(schedules_json, item);
  }
  saveJsonToNvs(handle, "schedules", schedules_json);
  cJSON_Delete(schedules_json);
    
  // Create JSON for band plan
  cJSON *bands_json = cJSON_CreateArray();
  for (int i = 0; i < config.numBandsInPlan; ++i) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "f", config.bandPlan[i].frequencyHz);
    cJSON_AddNumberToObject(item, "i", config.bandPlan[i].iterations);
    cJSON_AddItemToArray(bands_json, item);
  }
  saveJsonToNvs(handle, "bands", bands_json);
  cJSON_Delete(bands_json);

  err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

bool ConfigManager::loadStats(BeaconStats& stats) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) return false;
  size_t size = sizeof(stats.totalTxMinutes);
  nvs_get_blob(handle, "stats_tx", stats.totalTxMinutes, &size);
  nvs_close(handle);
  return true;
}

bool ConfigManager::saveStats(const BeaconStats& stats) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;
  nvs_set_blob(handle, "stats_tx", stats.totalTxMinutes, sizeof(stats.totalTxMinutes));
  err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

