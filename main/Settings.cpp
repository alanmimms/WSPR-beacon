#include "Settings.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include <stdlib.h>

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

const Setting* findSetting(const Settings* settingsObj, const char* key) {
    for (int i = 0; i < settingsObj->numSettings; ++i) {
        if (strcmp(settingsObj->settings[i].key, key) == 0) {
            return &settingsObj->settings[i];
        }
    }
    return nullptr;
}

Settings::Settings() {
    // Define all settings keys.
    settings[0] = {"hostname",  nullptr};
    settings[1] = {"ssid",      nullptr};
    settings[2] = {"password",  nullptr};
    settings[3] = {"callsign",  nullptr};
    settings[4] = {"locator",   nullptr};
    settings[5] = {"powerDbm",  nullptr};
    settings[6] = {"adminUser", nullptr};
    settings[7] = {"adminPass", nullptr};
    settings[8] = {"tx_percentage", nullptr};
    settings[9] = {"tx_skip",   nullptr};

    // Initialize with default values.
    setValue("hostname", "wspr-beacon");
    setValue("ssid", "");
    setValue("password", "");
    setValue("callsign", "N0CALL");
    setValue("locator", "AA00aa");
    setValue("powerDbm", "10");
    setValue("adminUser", "beacon");
    setValue("adminPass", "WSPR");
    setValue("tx_percentage", "20");
    setValue("tx_skip", "0");
}

Settings::~Settings() {
    for (int i = 0; i < numSettings; ++i) {
        if (settings[i].value != nullptr) {
            free(settings[i].value);
        }
    }
}

void Settings::load() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) { /* ... */ return; }

    for (int i = 0; i < numSettings; ++i) {
        size_t requiredLen = 0;
        err = nvs_get_str(nvsHandle, settings[i].key, NULL, &requiredLen);
        if (err == ESP_OK && requiredLen > 0) {
            char* tempValue = (char*)malloc(requiredLen);
            if (tempValue) {
                nvs_get_str(nvsHandle, settings[i].key, tempValue, &requiredLen);
                setValue(settings[i].key, tempValue);
                free(tempValue);
            }
        }
    }
    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Settings loaded from NVS.");
}

void Settings::save() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) { /* ... */ return; }

    for (int i = 0; i < numSettings; ++i) {
        nvs_set_str(nvsHandle, settings[i].key, settings[i].value);
    }

    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err)); }
    
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
        if (setting->value != nullptr) {
            free(setting->value);
        }
        setting->value = strdup(value);
    }
}
