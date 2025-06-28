#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_err.h"
#include <stddef.h>

class Settings {
public:
  Settings(const char *defaultJsonString);
  ~Settings();

  int getInt(const char *key, int defaultValue = 0) const;
  float getFloat(const char *key, float defaultValue = 0.0f) const;
  const char *getString(const char *key, const char *defaultValue = "") const;

  void setInt(const char *key, int value);
  void setFloat(const char *key, float value);
  void setString(const char *key, const char *value);

  esp_err_t storeToNVS();

  // Return a malloc'd JSON string representing the settings (caller free()s)
  char *toJsonString() const;

  // Parse a JSON string and update settings, returns true on success
  bool fromJsonString(const char *jsonString);

private:
  void *defaults;
  void *user;
  const char *defaultsString;

  void mergeDefaults();
  void loadFromNVS();

  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;
};

#endif // SETTINGS_H