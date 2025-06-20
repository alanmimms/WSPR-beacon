#ifndef SETTINGS_H
#define SETTINGS_H

#include "cJSON.h"
#include "esp_err.h"
#include <stddef.h>

class Settings {
public:
  // Construct with default JSON string (schema + defaults)
  Settings(const char *defaultJsonString);
  ~Settings();

  // Value getters: returns value or default if not present/type mismatch
  int getInt(const char *key, int defaultValue = 0) const;
  float getFloat(const char *key, float defaultValue = 0.0f) const;
  // Returns pointer to string value or default if not present/type mismatch
  const char *getString(const char *key, const char *defaultValue = "") const;

  // Value setters: sets value in cJSON and does not persist to NVS until storeToNVS is called
  void setInt(const char *key, int value);
  void setFloat(const char *key, float value);
  void setString(const char *key, const char *value);

  // Store the current JSON object to NVS
  esp_err_t storeToNVS();

  // Get JSON string representation of the current settings (merged with defaults).
  // buf must be large enough to hold the JSON string.
  // Returns ESP_OK on success.
  esp_err_t getJSON(char *buf, size_t buflen) const;

  // Direct const access for advanced use (read-only)
  const cJSON *getUserCJSON() const { return user; }

private:
  cJSON *defaults;
  cJSON *user;
  const char *defaultsString;

  void mergeDefaults();
  void loadFromNVS();
  esp_err_t internalGetJson(char *buf, size_t buflen) const;

  // No copying
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;
};

#endif // SETTINGS_H