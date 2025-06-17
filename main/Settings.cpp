#include "Settings.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include <stdlib.h> // For free()

static const char* TAG = "Settings";
static const char* NVS_NAMESPACE = "beacon_cfg";

// Helper function to find a setting struct by its key
static Setting* findSetting(Settings* settingsObj, const char* key) {
    for (int i = 0; i < settingsObj->numSettings; ++i) {
        if (strcmp(settingsObj->settings[i].key, key) == 0) {
            return &settingsObj->settings[i];
        }
    }
    return nullptr;
}

// Const-correct version of the helper function
static const Setting* findSetting(const Settings* settingsObj, const char* key) {
    for (int i = 0; i < settingsObj->numSettings; ++i) {
        if (strcmp(settingsObj->settings[i].key, key) == 0) {
            return &settingsObj->settings[i];
        }
    }
    return nullptr;
}


Settings::Settings() {
    // Initialize the keys for our settings array. Value pointers are null initially.
    settings[0] = {"hostname",  nullptr};
    settings[1] = {"ssid",      nullptr};
    settings[2] = {"password",  nullptr};
    settings[3] = {"callsign",  nullptr};
    settings[4] = {"locator",   nullptr};
    settings[5] = {"powerDbm",  nullptr};
    settings[6] = {"adminUser", nullptr};
    settings[7] = {"adminPass", nullptr};

    // Initialize with default values. setValue will handle memory allocation.
    setValue("hostname", "wspr-beacon");
    setValue("ssid", "");
    setValue("password", "");
    setValue("callsign", "N0CALL");
    setValue("locator", "AA00aa");
    setValue("powerDbm", "10");
    setValue("adminUser", "beacon");
    setValue("adminPass", "WSPR");
}

Settings::~Settings() {
    // Free the dynamically allocated memory for all setting values
    for (int i = 0; i < numSettings; ++i) {
        if (settings[i].value != nullptr) {
            free(settings[i].value);
        }
    }
}

void Settings::load() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    // Iterate through all settings and load them from NVS
    for (int i = 0; i < numSettings; ++i) {
        size_t requiredLen = 0;
        // First, get the length of the string in NVS
        err = nvs_get_str(nvsHandle, settings[i].key, NULL, &requiredLen);
        if (err == ESP_OK && requiredLen > 0) {
            char* tempValue = (char*)malloc(requiredLen);
            if (tempValue) {
                // Now, get the actual string
                nvs_get_str(nvsHandle, settings[i].key, tempValue, &requiredLen);
                // Use setValue to correctly manage memory
                setValue(settings[i].key, tempValue);
                free(tempValue);
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "NVS read error for key '%s': %s", settings[i].key, esp_err_to_name(err));
        }
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

    // Iterate through all settings and save them to NVS
    for (int i = 0; i < numSettings; ++i) {
        nvs_set_str(nvsHandle, settings[i].key, settings[i].value);
    }

    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Settings saved to NVS.");
}

const char* Settings::getValue(const char* key) const {
    const Setting* setting = findSetting(this, key);
    return setting ? setting->value : nullptr;
}

void Settings::setValue(const char* key, const char* value) {
    Setting* setting = findSetting(this, key);
    if (setting) {
        // Free the old value if it exists
        if (setting->value != nullptr) {
            free(setting->value);
        }
        // Allocate new memory and copy the new value. strdup = malloc + strcpy
        setting->value = strdup(value);
        if (setting->value == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate memory for setting '%s'", key);
        }
    } else {
        ESP_LOGW(TAG, "Attempted to set unknown setting: %s", key);
    }
}
