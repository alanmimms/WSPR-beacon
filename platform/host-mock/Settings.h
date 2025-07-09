#pragma once

#include "SettingsIntf.h"
#include "cJSON.h"

class Settings : public SettingsIntf {
public:
  Settings();
  ~Settings() override;

  int getInt(const char *key, int defaultValue = 0) const override;
  float getFloat(const char *key, float defaultValue = 0.0f) const override;
  const char *getString(const char *key, const char *defaultValue = "") const override;

  void setInt(const char *key, int value) override;
  void setFloat(const char *key, float value) override;
  void setString(const char *key, const char *value) override;

  bool store() override;

  char *toJsonString() const override;
  bool fromJsonString(const char *jsonString) override;

private:
  void* defaults;  // cJSON* stored as void* to avoid exposing cJSON in header
  void* user;      // cJSON* stored as void* to avoid exposing cJSON in header
  
  void mergeDefaults();
  void loadFromFile();
};