#include "Settings.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <cstdlib>
#include "cJSON.h"

#define SETTINGS_NAMESPACE "settings"
#define SETTINGS_KEY "json"
#define JSON_MAX_SIZE 4096

static const char *TAG = "Settings";
static const char *defaultSettingsJson = R"({
  "call": "N0CALL",
  "loc": "AA00aa",
  "pwr": 10,
  "txPct": 0,
  "txIntervalMinutes": 4,
  "bandMode": "sequential",
  "wifiMode": "sta",
  "ssid": "",
  "pwd": "",
  "bands": {
    "160m": {"en": 0, "freq": 1838100, "sched": 16777215},
    "80m": {"en": 0, "freq": 3570100, "sched": 16777215},
    "40m": {"en": 0, "freq": 7040100, "sched": 16777215},
    "30m": {"en": 0, "freq": 10140200, "sched": 16777215},
    "20m": {"en": 1, "freq": 14097100, "sched": 16777215},
    "17m": {"en": 0, "freq": 18106100, "sched": 16777215},
    "15m": {"en": 0, "freq": 21096100, "sched": 16777215},
    "12m": {"en": 0, "freq": 24926100, "sched": 16777215},
    "10m": {"en": 0, "freq": 28126100, "sched": 16777215}
  }
})";

Settings::Settings()
  : defaults(nullptr), user(nullptr) {
  defaults = cJSON_Parse(defaultSettingsJson);
  if (!defaults) {
    ESP_LOGE(TAG, "Default JSON is invalid!");
    defaults = cJSON_CreateObject();
  }
  loadFromNVS();
  mergeDefaults();
}

Settings::~Settings() {
  if (defaults) cJSON_Delete((cJSON *)defaults);
  if (user) cJSON_Delete((cJSON *)user);
}

void Settings::mergeDefaults() {
  if (!user || !defaults) return;
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, (cJSON *)defaults) {
    const char *key = it->string;
    cJSON *userItem = cJSON_GetObjectItem((cJSON *)user, key);
    if (!userItem || userItem->type != it->type) {
      if (userItem) cJSON_DeleteItemFromObject((cJSON *)user, key);
      cJSON_AddItemToObject((cJSON *)user, key, cJSON_Duplicate(it, 1));
    }
  }
}

void Settings::loadFromNVS() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "loadFromNVS nvs_open failed - creating empty object");
    user = cJSON_CreateObject();
    return;
  }

  size_t requiredLen = 0;
  err = nvs_get_str(handle, SETTINGS_KEY, NULL, &requiredLen);
  if (err != ESP_OK || requiredLen == 0) {
    ESP_LOGI(TAG, "loadFromNVS nvs_get_str failed - creating empty object");
    nvs_close(handle);
    user = cJSON_CreateObject();
    return;
  }

  char *buf = (char *)malloc(requiredLen);
  if (!buf) {
    ESP_LOGI(TAG, "loadFromNVS malloc failed - creating empty object");
    nvs_close(handle);
    user = cJSON_CreateObject();
    return;
  }

  err = nvs_get_str(handle, SETTINGS_KEY, buf, &requiredLen);
  nvs_close(handle);

  ESP_LOGI(TAG, "LOAD Settings JSON: %s", buf);

  if (err == ESP_OK && buf[0] != '\0') {
    user = cJSON_Parse(buf);
    if (!user) {
      ESP_LOGW(TAG, "Invalid user JSON in NVS, using empty object");
      user = cJSON_CreateObject();
    }
  } else {
    ESP_LOGI(TAG, "loadFromNVS JSON parse failed - creating empty object");
    user = cJSON_CreateObject();
  }

  free(buf);
}

bool Settings::store() {
  char *json = toJsonString();
  if (!json) return false;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "storeToNVS nvs_open failed: %s", esp_err_to_name(err));
    free(json);
    return false;
  }

  ESP_LOGI(TAG, "STORE Settings JSON: %s", json);

  err = nvs_set_str(handle, SETTINGS_KEY, json);

  if (err == ESP_OK) {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "storeToNVS nvs_commit failed: %s", esp_err_to_name(err));
    }
  } else {
    ESP_LOGE(TAG, "storeToNVS nvs_set_str failed: %s", esp_err_to_name(err)); 
  }
  
  nvs_close(handle);
  free(json);
  return err == ESP_OK;
}

int Settings::getInt(const char *key, int defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  return defaultValue;
}

float Settings::getFloat(const char *key, float defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  return defaultValue;
}

const char *Settings::getString(const char *key, const char *defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  return defaultValue;
}

void Settings::setInt(const char *key, int value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject((cJSON *)user, key, value);
  }
}

void Settings::setFloat(const char *key, float value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject((cJSON *)user, key, value);
  }
}

void Settings::setString(const char *key, const char *value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateString(value));
  } else {
    cJSON_AddStringToObject((cJSON *)user, key, value);
  }
}

char *Settings::toJsonString() const {
  cJSON *merged = cJSON_Duplicate((cJSON *)defaults, 1);
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, (cJSON *)user) {
    cJSON *inMerged = cJSON_GetObjectItem(merged, it->string);
    if (inMerged && inMerged->type == it->type) {
      cJSON_ReplaceItemInObject(merged, it->string, cJSON_Duplicate(it, 1));
    } else if (!inMerged) {
      cJSON_AddItemToObject(merged, it->string, cJSON_Duplicate(it, 1));
    }
  }
  char *json = cJSON_PrintUnformatted(merged);
  cJSON_Delete(merged);
  return json;
}

bool Settings::fromJsonString(const char *jsonString) {
  if (!user) return false;
  
  ESP_LOGI(TAG, "fromJsonString: Parsing JSON: %s", jsonString);
  
  cJSON *parsed = cJSON_Parse(jsonString);
  if (!parsed) {
    ESP_LOGE(TAG, "fromJsonString: Failed to parse JSON");
    return false;
  }
  
  // Clear existing user settings to start fresh
  cJSON_Delete((cJSON *)user);
  user = cJSON_CreateObject();
  
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, parsed) {
    const char *key = it->string;
    ESP_LOGI(TAG, "fromJsonString: Processing key: %s", key);
    
    if (cJSON_IsString(it)) {
      ESP_LOGI(TAG, "  Setting string: %s = %s", key, it->valuestring);
      setString(key, it->valuestring);
    } else if (cJSON_IsNumber(it)) {
      ESP_LOGI(TAG, "  Setting number: %s = %f", key, it->valuedouble);
      setFloat(key, (float)it->valuedouble);
    } else if (cJSON_IsObject(it)) {
      // Handle nested objects (like bands)
      ESP_LOGI(TAG, "  Setting object: %s", key);
      cJSON_AddItemToObject((cJSON *)user, key, cJSON_Duplicate(it, 1));
    }
  }
  
  cJSON_Delete(parsed);
  ESP_LOGI(TAG, "fromJsonString: Completed successfully");
  return true;
}