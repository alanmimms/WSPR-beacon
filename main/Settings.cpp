#include "Settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <cstring>
#include <stdio.h>

#define SETTINGS_NAMESPACE "settings"
#define KEY_SSID "ssid"
#define KEY_PASSWORD "password"
#define KEY_HOSTNAME "hostname"

#define KEY_CALLSIGN "callsign"
#define KEY_LOCATOR "locator"
#define KEY_POWERMW "powerMW"
#define KEY_POWERDBM "powerDbm"

static const char *DEFAULT_CALLSIGN = "NOCALL";
static const char *DEFAULT_LOCATOR = "AA00aa";
static const int DEFAULT_POWERMW = 100;
static const int DEFAULT_POWERDBM = 23;

Settings::Settings() {}

Settings::~Settings() {}

void Settings::load() {
  // Try to read each setting from NVS. If any are missing, write defaults for all.
  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  bool needDefaults = err != ESP_OK;

  char callsign[32];
  size_t len = sizeof(callsign);
  if (nvs_get_str(nvsHandle, KEY_CALLSIGN, callsign, &len) != ESP_OK) {
    needDefaults = true;
  }

  char locator[16];
  len = sizeof(locator);
  if (nvs_get_str(nvsHandle, KEY_LOCATOR, locator, &len) != ESP_OK) {
    needDefaults = true;
  }

  int32_t powerMW;
  if (nvs_get_i32(nvsHandle, KEY_POWERMW, &powerMW) != ESP_OK) {
    needDefaults = true;
  }

  int32_t powerDbm;
  if (nvs_get_i32(nvsHandle, KEY_POWERDBM, &powerDbm) != ESP_OK) {
    needDefaults = true;
  }

  if (needDefaults) {
    // Write defaults
    nvs_set_str(nvsHandle, KEY_CALLSIGN, DEFAULT_CALLSIGN);
    nvs_set_str(nvsHandle, KEY_LOCATOR, DEFAULT_LOCATOR);
    nvs_set_i32(nvsHandle, KEY_POWERMW, DEFAULT_POWERMW);
    nvs_set_i32(nvsHandle, KEY_POWERDBM, DEFAULT_POWERDBM);
    nvs_commit(nvsHandle);

    // Immediately save JSON blob for defaults
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, KEY_CALLSIGN, DEFAULT_CALLSIGN);
    cJSON_AddStringToObject(root, KEY_LOCATOR, DEFAULT_LOCATOR);
    cJSON_AddNumberToObject(root, KEY_POWERMW, DEFAULT_POWERMW);
    cJSON_AddNumberToObject(root, KEY_POWERDBM, DEFAULT_POWERDBM);
    char *jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
      // Save JSON to NVS under "settingsJson" for reference/debug (optional)
      nvs_set_str(nvsHandle, "settingsJson", jsonStr);
      nvs_commit(nvsHandle);
      cJSON_free(jsonStr);
    }
    cJSON_Delete(root);
  }

  nvs_close(nvsHandle);
}

esp_err_t Settings::saveJson(const char *json) {
  cJSON *root = cJSON_Parse(json);
  if (!root) return ESP_FAIL;

  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) goto OPENFAIL;

  nvs_set_str(nvsHandle, "settingsJson", json);

  err = nvs_commit(nvsHandle);
  nvs_close(nvsHandle);
 OPENFAIL:
  cJSON_Delete(root);
  return err;
}

esp_err_t Settings::getJson(char *buf, size_t buflen) {
  cJSON *root = cJSON_CreateObject();

  char callsign[32] = "";
  char locator[16] = "";
  int powerMW = 0;
  int powerDbm = 0;

  getString(KEY_CALLSIGN, callsign, sizeof(callsign), DEFAULT_CALLSIGN);
  getString(KEY_LOCATOR, locator, sizeof(locator), DEFAULT_LOCATOR);
  powerMW = getInt(KEY_POWERMW, DEFAULT_POWERMW);
  powerDbm = getInt(KEY_POWERDBM, DEFAULT_POWERDBM);

  cJSON_AddStringToObject(root, KEY_CALLSIGN, callsign);
  cJSON_AddStringToObject(root, KEY_LOCATOR, locator);
  cJSON_AddNumberToObject(root, KEY_POWERMW, powerMW);
  cJSON_AddNumberToObject(root, KEY_POWERDBM, powerDbm);

  char *rendered = cJSON_PrintUnformatted(root);
  if (rendered && strlen(rendered) < buflen) {
    strcpy(buf, rendered);
    cJSON_free(rendered);
    cJSON_Delete(root);
    return ESP_OK;
  }
  if (rendered) cJSON_free(rendered);
  cJSON_Delete(root);
  return ESP_FAIL;
}

int Settings::getInt(const char *key, int defaultValue) {
  nvs_handle_t nvsHandle;
  int value = defaultValue;
  if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvsHandle) == ESP_OK) {
    int32_t temp = 0;
    if (nvs_get_i32(nvsHandle, key, &temp) == ESP_OK) value = temp;
    nvs_close(nvsHandle);
  }
  return value;
}

void Settings::setInt(const char *key, int value) {
  nvs_handle_t nvsHandle;
  if (nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvsHandle) == ESP_OK) {
    nvs_set_i32(nvsHandle, key, value);
    nvs_commit(nvsHandle);
    nvs_close(nvsHandle);
  }
}

void Settings::getString(const char *key, char *dst, size_t dstLen, const char *defaultValue) {
  nvs_handle_t nvsHandle;
  size_t required = 0;
  dst[0] = 0;
  if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvsHandle) == ESP_OK) {
    if (nvs_get_str(nvsHandle, key, NULL, &required) == ESP_OK && required < dstLen) {
      nvs_get_str(nvsHandle, key, dst, &required);
    } else {
      strncpy(dst, defaultValue, dstLen - 1);
      dst[dstLen - 1] = 0;
    }
    nvs_close(nvsHandle);
  } else {
    strncpy(dst, defaultValue, dstLen - 1);
    dst[dstLen - 1] = 0;
  }
}

void Settings::setString(const char *key, const char *value) {
  nvs_handle_t nvsHandle;
  if (nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvsHandle) == ESP_OK) {
    nvs_set_str(nvsHandle, key, value);
    nvs_commit(nvsHandle);
    nvs_close(nvsHandle);
  }
}
