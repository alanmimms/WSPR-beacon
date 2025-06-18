#pragma once

#include "esp_err.h"

typedef struct {
  char callsign[16];
  char locator[8];
  float dbm;
} BeaconSettings;

esp_err_t getSettingsJson(char *buf, size_t buflen);
esp_err_t saveSettingsJson(const char *json);
extern BeaconSettings settings;