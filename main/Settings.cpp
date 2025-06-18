#include "Settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>
#include <math.h>

// Define NVS key constants if not already defined elsewhere
#define NVS_NAMESPACE "settings"
#define NVS_JSON_KEY  "settings_json"

static const char *TAG = "Settings";
static cJSON *gSettingsJson = NULL;
static SemaphoreHandle_t gSettingsMutex;

BeaconSettings settings;

static float wattsToDbm(float watts) {
  if (watts <= 0) return 0.0f;
  return 10.0f * log10f(watts * 1000.0f);
}

static float dbmToWatts(float dbm) {
  return powf(10.0f, dbm / 10.0f) / 1000.0f;
}

static void ensureDefaultSettings() {
  if (!cJSON_HasObjectItem(gSettingsJson, "callsign"))
    cJSON_AddStringToObject(gSettingsJson, "callsign", "N0CALL");
  if (!cJSON_HasObjectItem(gSettingsJson, "locator"))
    cJSON_AddStringToObject(gSettingsJson, "locator", "AA00aa");
  if (!cJSON_HasObjectItem(gSettingsJson, "dbm"))
    cJSON_AddNumberToObject(gSettingsJson, "dbm", 30.0f);
}

void initializeSettings() {
  gSettingsMutex = xSemaphoreCreateMutex();
  gSettingsJson = cJSON_CreateObject();
  if (!gSettingsJson) {
    ESP_LOGE(TAG, "Failed to create cJSON object during initialization!");
  }
}

esp_err_t loadSettings() {
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
    char *jsonString = (char *)malloc(requiredSize);
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

  strncpy(settings.callsign, cJSON_GetObjectItem(gSettingsJson, "callsign")->valuestring, sizeof(settings.callsign) - 1);
  strncpy(settings.locator, cJSON_GetObjectItem(gSettingsJson, "locator")->valuestring, sizeof(settings.locator) - 1);
  settings.dbm = (float)cJSON_GetObjectItem(gSettingsJson, "dbm")->valuedouble;

  nvs_close(nvsHandle);
  xSemaphoreGive(gSettingsMutex);
  return ESP_OK;
}

esp_err_t saveSettingsJson(const char *jsonString) {
  if (!jsonString) return ESP_ERR_INVALID_ARG;
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;

  cJSON *newJson = cJSON_Parse(jsonString);
  if (!newJson) {
    ESP_LOGE(TAG, "Failed to parse incoming JSON string for saving.");
    xSemaphoreGive(gSettingsMutex);
    return ESP_FAIL;
  }

  cJSON *callsign = cJSON_GetObjectItem(newJson, "callsign");
  cJSON *locator = cJSON_GetObjectItem(newJson, "locator");
  cJSON *dbm = cJSON_GetObjectItem(newJson, "dbm");

  cJSON_Delete(gSettingsJson);
  gSettingsJson = cJSON_CreateObject();
  if (callsign) cJSON_AddStringToObject(gSettingsJson, "callsign", callsign->valuestring);
  if (locator) cJSON_AddStringToObject(gSettingsJson, "locator", locator->valuestring);
  if (dbm) cJSON_AddNumberToObject(gSettingsJson, "dbm", dbm->valuedouble);

  strncpy(settings.callsign, callsign ? callsign->valuestring : "N0CALL", sizeof(settings.callsign) - 1);
  strncpy(settings.locator, locator ? locator->valuestring : "AA00aa", sizeof(settings.locator) - 1);
  settings.dbm = dbm ? (float)dbm->valuedouble : 30.0f;

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
      nvs_commit(nvsHandle);
    }
    nvs_close(nvsHandle);
  }
  free(compactString);

  xSemaphoreGive(gSettingsMutex);
  return err;
}

esp_err_t getSettingsJson(char *buf, size_t buflen) {
  if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "callsign", settings.callsign);
  cJSON_AddStringToObject(out, "locator", settings.locator);
  cJSON_AddNumberToObject(out, "dbm", settings.dbm);

  char *jsonStr = cJSON_PrintUnformatted(out);
  if (jsonStr && strlen(jsonStr) < buflen) {
    strcpy(buf, jsonStr);
    cJSON_free(jsonStr);
    cJSON_Delete(out);
    xSemaphoreGive(gSettingsMutex);
    return ESP_OK;
  }
  if (jsonStr) cJSON_free(jsonStr);
  cJSON_Delete(out);
  xSemaphoreGive(gSettingsMutex);
  return ESP_FAIL;
}