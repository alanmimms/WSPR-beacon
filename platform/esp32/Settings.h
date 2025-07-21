#pragma once

#include "SettingsBase.h"
#include <stddef.h>
#include <esp_err.h>

class Settings : public SettingsBase {
public:
  Settings();
  ~Settings() override;

  // Platform-specific storage implementation
  bool loadFromStorage() override;
  bool saveToStorage() override;

protected:
  // Platform-specific logging implementation
  void logInfo(const char* format, ...) override;
  void logError(const char* format, ...) override;

private:
  // ESP32-specific NVS methods
  bool loadFromNVS();
  bool saveToNVS();
};