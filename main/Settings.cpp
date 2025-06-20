#include "Settings.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <cstdlib>

#define SETTINGS_NAMESPACE "settings"
#define SETTINGS_KEY "json"
#define JSON_MAX_SIZE 1024

static const char *TAG = "Settings";

Settings::Settings(const char *defaultJsonString)
  : defaults(nullptr), user(nullptr), defaultsString(defaultJsonString) {
  defaults = cJSON_Parse(defaultJsonString);
  if (!defaults) {
    ESP_LOGE(TAG, "Default JSON is invalid!");
    defaults = cJSON_CreateObject();
  }

  loadFromNVS();
  mergeDefaults();
}

Settings::~Settings() {
  if (defaults) cJSON_Delete(defaults);
  if (user) cJSON_Delete(user);
}

void Settings::mergeDefaults() {
  if (!user || !defaults) return;
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, defaults) {
    const char *key = it->string;
    cJSON *userItem = cJSON_GetObjectItem(user, key);
    if (!userItem || userItem->type != it->type) {
      if (userItem) cJSON_DeleteItemFromObject(user, key);
      cJSON_AddItemToObject(user, key, cJSON_Duplicate(it, 1));
    }
  }
}

void Settings::loadFromNVS() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    user = cJSON_CreateObject();
    return;
  }

  // Get required length
  size_t requiredLen = 0;
  err = nvs_get_str(handle, SETTINGS_KEY, NULL, &requiredLen);
  if (err != ESP_OK || requiredLen == 0) {
    nvs_close(handle);
    user = cJSON_CreateObject();
    return;
  }

  char *buf = (char *)malloc(requiredLen);
  if (!buf) {
    nvs_close(handle);
    user = cJSON_CreateObject();
    return;
  }

  err = nvs_get_str(handle, SETTINGS_KEY, buf, &requiredLen);
  nvs_close(handle);

  if (err == ESP_OK && buf[0] != '\0') {
    user = cJSON_Parse(buf);
    if (!user) {
      ESP_LOGW(TAG, "Invalid user JSON in NVS, using empty object");
      user = cJSON_CreateObject();
    }
  } else {
    user = cJSON_CreateObject();
  }

  free(buf);
}

esp_err_t Settings::storeToNVS() {
  char temp[JSON_MAX_SIZE];
  if (internalGetJson(temp, sizeof(temp)) != ESP_OK) return ESP_FAIL;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_str(handle, SETTINGS_KEY, temp);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t Settings::internalGetJson(char *buf, size_t buflen) const {
  if (!user || !buf) return ESP_FAIL;
  cJSON *merged = cJSON_Duplicate(defaults, 1);
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, user) {
    cJSON *inMerged = cJSON_GetObjectItem(merged, it->string);
    if (inMerged && inMerged->type == it->type) {
      cJSON_ReplaceItemInObject(merged, it->string, cJSON_Duplicate(it, 1));
    } else if (!inMerged) {
      cJSON_AddItemToObject(merged, it->string, cJSON_Duplicate(it, 1));
    }
  }
  char *json = cJSON_PrintUnformatted(merged);
  size_t len = strlen(json);
  if (len >= buflen) {
    cJSON_free(json);
    cJSON_Delete(merged);
    return ESP_FAIL;
  }
  strcpy(buf, json);
  cJSON_free(json);
  cJSON_Delete(merged);
  return ESP_OK;
}

esp_err_t Settings::getJSON(char *buf, size_t buflen) const {
  return internalGetJson(buf, buflen);
}

// --- Getters ---

int Settings::getInt(const char *key, int defaultValue) const {
  cJSON *it = cJSON_GetObjectItem(user, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  it = cJSON_GetObjectItem(defaults, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  return defaultValue;
}

float Settings::getFloat(const char *key, float defaultValue) const {
  cJSON *it = cJSON_GetObjectItem(user, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  it = cJSON_GetObjectItem(defaults, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  return defaultValue;
}

const char *Settings::getString(const char *key, const char *defaultValue) const {
  cJSON *it = cJSON_GetObjectItem(user, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  it = cJSON_GetObjectItem(defaults, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  return defaultValue;
}

// --- Setters ---

void Settings::setInt(const char *key, int value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem(user, key);
  if (item) {
    cJSON_ReplaceItemInObject(user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject(user, key, value);
  }
}

void Settings::setFloat(const char *key, float value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem(user, key);
  if (item) {
    cJSON_ReplaceItemInObject(user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject(user, key, value);
  }
}

void Settings::setString(const char *key, const char *value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem(user, key);
  if (item) {
    cJSON_ReplaceItemInObject(user, key, cJSON_CreateString(value));
  } else {
    cJSON_AddStringToObject(user, key, value);
  }
}