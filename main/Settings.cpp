#include "Settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char *TAG = "Settings";
static cJSON *gSettingsJson = NULL;
static SemaphoreHandle_t gSettingsMutex;

Settings::Settings() {
  // Constructor does not initialize mutex or JSON object; call initialize() explicitly
}

void Settings::initialize() {
  gSettingsMutex = xSemaphoreCreateMutex();
  gSettingsJson = cJSON_CreateObject();
  if (!gSettingsJson) {
    ESP_LOGE(TAG, "Failed to create cJSON object during initialization!");
  }
}

esp_err_t Settings::load() {
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;

  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
    xSemaphoreGive(gSettingsMutex);
    return err;
  }

  size_t requiredSize = 0;
  err = nvs_get_str(nvsHandle, NVS_JSON_KEY, NULL, &requiredSize);

  bool loadSuccess = false;
  if (err == ESP_OK && requiredSize > 1) {
    char *jsonString = (char *) malloc(requiredSize);
    if (jsonString) {
      err = nvs_get_str(nvsHandle, NVS_JSON_KEY, jsonString, &requiredSize);
      if (err == ESP_OK) {
        cJSON_Delete(gSettingsJson);
        gSettingsJson = cJSON_Parse(jsonString);
        if (gSettingsJson != NULL) {
          ESP_LOGI(TAG, "Successfully loaded and parsed settings from NVS.");
          loadSuccess = true;
        }
      }
      free(jsonString);
    }
  }

  if (!loadSuccess) {
    ESP_LOGW(TAG, "Failed to load settings from NVS or JSON was invalid. Applying defaults.");
    if (gSettingsJson == NULL) {
      gSettingsJson = cJSON_CreateObject();
    }
    ensureDefaultSettings();
  }

  nvs_close(nvsHandle);
  xSemaphoreGive(gSettingsMutex);
  return ESP_OK;
}

esp_err_t Settings::saveJson(const char *jsonString) {
  if (!jsonString) return ESP_ERR_INVALID_ARG;
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;

  cJSON *newJson = cJSON_Parse(jsonString);
  if (!newJson) {
    ESP_LOGE(TAG, "Failed to parse incoming JSON string for saving.");
    xSemaphoreGive(gSettingsMutex);
    return ESP_FAIL;
  }

  cJSON_Delete(gSettingsJson);
  gSettingsJson = newJson;

  char *compactString = cJSON_PrintUnformatted(gSettingsJson);
  if (!compactString) {
    ESP_LOGE(TAG, "Failed to print compact JSON for NVS.");
    xSemaphoreGive(gSettingsMutex);
    return ESP_FAIL;
  }

  nvs_handle_t nvsHandle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err == ESP_OK) {
    err = nvs_set_str(nvsHandle, NVS_JSON_KEY, compactString);
    if (err == ESP_OK) {
      err = nvs_commit(nvsHandle);
    }
    nvs_close(nvsHandle);
  }
  cJSON_free(compactString);
  xSemaphoreGive(gSettingsMutex);
  return err;
}

esp_err_t Settings::getJson(char *buffer, size_t len) {
  if (!buffer || len == 0) return ESP_ERR_INVALID_ARG;
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;

  char *jsonString = cJSON_PrintUnformatted(gSettingsJson);
  if (!jsonString) {
    xSemaphoreGive(gSettingsMutex);
    return ESP_FAIL;
  }
  size_t strLen = strlen(jsonString);
  if (strLen + 1 > len) {
    cJSON_free(jsonString);
    xSemaphoreGive(gSettingsMutex);
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  strncpy(buffer, jsonString, len - 1);
  buffer[len - 1] = 0;
  cJSON_free(jsonString);
  xSemaphoreGive(gSettingsMutex);
  return ESP_OK;
}

size_t Settings::getString(const char *key, char *value, size_t maxLen, const char *defaultValue) {
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) {
    if (defaultValue && value && maxLen > 0) {
      strncpy(value, defaultValue, maxLen - 1);
      value[maxLen - 1] = 0;
      return strlen(value);
    }
    return 0;
  }
  if (!key || !value || maxLen == 0) {
    xSemaphoreGive(gSettingsMutex);
    return 0;
  }
  const cJSON *item = cJSON_GetObjectItem(gSettingsJson, key);
  if (item && cJSON_IsString(item) && item->valuestring) {
    strncpy(value, item->valuestring, maxLen - 1);
    value[maxLen - 1] = 0;
  } else if (defaultValue) {
    strncpy(value, defaultValue, maxLen - 1);
    value[maxLen - 1] = 0;
  } else if (maxLen > 0) {
    value[0] = 0;
  }
  xSemaphoreGive(gSettingsMutex);
  return strlen(value);
}

int Settings::getInt(const char *key, int defaultValue) {
  int result = defaultValue;
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) {
    return result;
  }
  if (!key) {
    xSemaphoreGive(gSettingsMutex);
    return result;
  }
  const cJSON *item = cJSON_GetObjectItem(gSettingsJson, key);
  if (item && cJSON_IsNumber(item)) {
    result = item->valueint;
  }
  xSemaphoreGive(gSettingsMutex);
  return result;
}

void Settings::ensureDefaultSettings() {
  // Fill in defaults as needed for your application
  cJSON_ReplaceItemInObject(gSettingsJson, "callsign", cJSON_CreateString("N0CALL"));
  cJSON_ReplaceItemInObject(gSettingsJson, "grid", cJSON_CreateString("AA00aa"));
  cJSON_ReplaceItemInObject(gSettingsJson, "powerDBm", cJSON_CreateNumber(10));
  cJSON_ReplaceItemInObject(gSettingsJson, "txIntervalMinutes", cJSON_CreateNumber(10));
}