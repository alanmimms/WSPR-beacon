#include "Settings.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cstdarg>

static const char* TAG = "Settings";
static const char* NVS_NAMESPACE = "wspr_settings";
static const char* NVS_KEY = "config";

Settings::Settings() : SettingsBase() {
    // Initialize after construction to avoid calling virtual functions in constructor
    initialize();
}

Settings::~Settings() {
    // Base class destructor handles cleanup
}

bool Settings::loadFromStorage() {
    return loadFromNVS();
}

bool Settings::saveToStorage() {
    return saveToNVS();
}

bool Settings::loadFromNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved settings found in NVS");
        } else {
            ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        }
        return false;
    }

    size_t length = 0;
    err = nvs_get_blob(handle, NVS_KEY, nullptr, &length);
    if (err != ESP_OK || length == 0) {
        nvs_close(handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No settings blob found in NVS");
        } else {
            ESP_LOGE(TAG, "Error getting blob size: %s", esp_err_to_name(err));
        }
        return false;
    }

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to allocate memory for settings");
        return false;
    }

    err = nvs_get_blob(handle, NVS_KEY, buffer, &length);
    nvs_close(handle);

    if (err != ESP_OK) {
        free(buffer);
        ESP_LOGE(TAG, "Error reading settings from NVS: %s", esp_err_to_name(err));
        return false;
    }

    buffer[length] = '\0';
    bool success = fromJsonString(buffer);
    free(buffer);

    if (success) {
        ESP_LOGI(TAG, "Settings loaded from NVS");
    } else {
        ESP_LOGE(TAG, "Failed to parse settings from NVS");
    }

    return success;
}

bool Settings::saveToNVS() {
    char* jsonStr = toJsonString();
    if (!jsonStr) {
        ESP_LOGE(TAG, "Failed to serialize settings");
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        free(jsonStr);
        ESP_LOGE(TAG, "Error opening NVS for writing: %s", esp_err_to_name(err));
        return false;
    }

    size_t length = strlen(jsonStr);
    err = nvs_set_blob(handle, NVS_KEY, jsonStr, length);
    free(jsonStr);

    if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Error writing settings to NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing settings to NVS: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Settings saved to NVS");
    return true;
}

void Settings::logInfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_INFO, TAG, format, args);
    va_end(args);
}

void Settings::logError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_ERROR, TAG, format, args);
    va_end(args);
}