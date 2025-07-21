#pragma once

#include "SettingsBase.h"

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
  // Host-mock specific file methods
  bool loadFromFile();
  bool saveToFile();
  
  static const char* SETTINGS_FILE;
};