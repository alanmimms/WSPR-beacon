#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_err.h"
#include <cstddef>

// --- Constants for setting value lengths ---
// These define the buffer sizes needed when retrieving settings.
#define MAX_CALLSIGN_LEN    32
#define MAX_GRID_LEN        12
#define MAX_SSID_LEN        32
#define MAX_PASSWORD_LEN    64

// --- NVS Storage Constants ---
#define NVS_NAMESPACE "jtencode_cfg"
#define NVS_JSON_KEY "settingsJson"

/**
 * @brief Initializes the settings module.
 *
 * Must be called once on startup before any other settings functions.
 * It prepares the in-memory store for settings.
 */
void initializeSettings();

/**
 * @brief Loads settings from NVS into memory.
 *
 * If settings are not found in NVS, it creates a default set.
 * @return ESP_OK on success.
 */
esp_err_t loadSettings();

/**
 * @brief Saves a JSON string to NVS and updates the active settings.
 *
 * The provided string is validated as JSON before saving.
 *
 * @param jsonString A null-terminated string containing the settings in JSON format.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t saveSettingsJson(const char* jsonString);

/**
 * @brief Retrieves the current settings as a JSON string.
 *
 * @param buffer A character buffer to write the JSON string into.
 * @param len The size of the buffer.
 * @return ESP_OK on success, or an error code if the buffer is too small.
 */
esp_err_t getSettingsJson(char* buffer, size_t len);

/**
 * @brief Gets a string value from the settings by its key.
 *
 * This is a thread-safe helper function.
 *
 * @param key The setting key (e.g., "callsign").
 * @param value A character buffer to write the value into.
 * @param maxLen The size of the value buffer.
 * @param defaultValue A default value to use if the key is not found.
 * @return The length of the string copied into the value buffer.
 */
size_t getSettingString(const char* key, char* value, size_t maxLen, const char* defaultValue);

/**
 * @brief Gets an integer value from the settings by its key.
 *
 * This is a thread-safe helper function.
 *
 * @param key The setting key (e.g., "powerDBm").
 * @param defaultValue A default value to use if the key is not found.
 * @return The integer value of the setting.
 */
int getSettingInt(const char* key, int defaultValue);

#endif // SETTINGS_H
