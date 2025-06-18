#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_err.h"
#include <cstddef>

// --- Constants for setting value lengths ---
#define MAX_CALLSIGN_LEN    32
#define MAX_GRID_LEN        12
#define MAX_SSID_LEN        32
#define MAX_PASSWORD_LEN    64

// --- NVS Storage Constants ---
#define NVS_NAMESPACE "jtencode_cfg"
#define NVS_JSON_KEY "settingsJson"

class Settings {
public:
  Settings();
  void initialize();
  esp_err_t load();
  esp_err_t saveJson(const char *jsonString);
  esp_err_t getJson(char *buffer, size_t len);
  size_t getString(const char *key, char *value, size_t maxLen, const char *defaultValue);
  int getInt(const char *key, int defaultValue);

private:
  void ensureDefaultSettings();
};

#endif // SETTINGS_H