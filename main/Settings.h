#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_err.h"

class Settings {
public:
  Settings();
  ~Settings();

  void load();
  esp_err_t saveJson(const char *json);
  esp_err_t getJson(char *buf, size_t buflen);

  int getInt(const char *key, int defaultValue);
  void setInt(const char *key, int value);

  void getString(const char *key, char *dst, size_t dstLen, const char *defaultValue);
  void setString(const char *key, const char *value);

  static constexpr int maxCallsignLen = 12;
  static constexpr int maxGridLen = 8;

private:
  // (Private data/methods if needed)
};

#endif // SETTINGS_H
