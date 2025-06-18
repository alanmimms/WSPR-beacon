#include "Settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "Settings";
static cJSON* gSettingsJson = NULL;
static SemaphoreHandle_t gSettingsMutex;

// Forward declaration for internal use
static void ensureDefaultSettings();

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
    if (err == ESP_OK && requiredSize > 1) { // >1 to ensure it's not an empty string
        char* jsonString = (char*)malloc(requiredSize);
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
            gSettingsJson = cJSON_CreateObject(); // Ensure we have a valid object
        }
        ensureDefaultSettings();
    }

    nvs_close(nvsHandle);
    xSemaphoreGive(gSettingsMutex);
    return ESP_OK;
}

esp_err_t saveSettingsJson(const char* jsonString) {
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
            ESP_LOGI(TAG, "Successfully saved settings to NVS.");
        }
        nvs_close(nvsHandle);
    }
    
    free(compactString);
    xSemaphoreGive(gSettingsMutex);
    return err;
}

esp_err_t getSettingsJson(char* buffer, size_t len) {
    if (!buffer || len == 0) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;
    
    if (!cJSON_PrintPreallocated(gSettingsJson, buffer, len, 0)) { // 0 = not formatted
        ESP_LOGE(TAG, "Failed to print JSON to buffer, likely insufficient size.");
        xSemaphoreGive(gSettingsMutex);
        return ESP_FAIL;
    }

    xSemaphoreGive(gSettingsMutex);
    return ESP_OK;
}

size_t getSettingString(const char* key, char* value, size_t maxLen, const char* defaultValue) {
    if (!key || !value || maxLen == 0) return 0;
    if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return 0;

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(gSettingsJson, key);
    size_t copiedLen = 0;
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        strncpy(value, item->valuestring, maxLen - 1);
        value[maxLen - 1] = '\0';
        copiedLen = strlen(value);
    } else if (defaultValue) {
        strncpy(value, defaultValue, maxLen - 1);
        value[maxLen - 1] = '\0';
        copiedLen = strlen(value);
    } else {
        value[0] = '\0';
    }

    xSemaphoreGive(gSettingsMutex);
    return copiedLen;
}

int getSettingInt(const char* key, int defaultValue) {
    if (!key) return defaultValue;
    if (xSemaphoreTake(gSettingsMutex, portMAX_DELAY) != pdTRUE) return defaultValue;
    
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(gSettingsJson, key);
    int result = defaultValue;
    if (cJSON_IsNumber(item)) {
        result = item->valueint;
    }

    xSemaphoreGive(gSettingsMutex);
    return result;
}

// Internal function, must be called within a mutex lock
static void ensureDefaultSettings() {
    if (!gSettingsJson) gSettingsJson = cJSON_CreateObject();
    if (!cJSON_GetObjectItem(gSettingsJson, "callsign")) cJSON_AddStringToObject(gSettingsJson, "callsign", "N0CALL");
    if (!cJSON_GetObjectItem(gSettingsJson, "grid")) cJSON_AddStringToObject(gSettingsJson, "grid", "AA00aa");
    if (!cJSON_GetObjectItem(gSettingsJson, "powerDBm")) cJSON_AddNumberToObject(gSettingsJson, "powerDBm", 10);
    if (!cJSON_GetObjectItem(gSettingsJson, "txIntervalMinutes")) cJSON_AddNumberToObject(gSettingsJson, "txIntervalMinutes", 10);
    if (!cJSON_GetObjectItem(gSettingsJson, "ssid")) cJSON_AddStringToObject(gSettingsJson, "ssid", "");
    if (!cJSON_GetObjectItem(gSettingsJson, "password")) cJSON_AddStringToObject(gSettingsJson, "password", "");
}
