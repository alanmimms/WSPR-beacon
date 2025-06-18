#include "Scheduler.h"
#include "Settings.h"
#include "esp_log.h"
#include <cstring>

// Define these here or in a shared header if not already defined
#define MAX_CALLSIGN_LEN 16
#define MAX_GRID_LEN 8

void Scheduler::transmit() {
  char callsign[MAX_CALLSIGN_LEN];
  char grid[MAX_GRID_LEN];

  // Copy callsign and locator from global settings struct
  strncpy(callsign, settings.callsign, MAX_CALLSIGN_LEN - 1);
  callsign[MAX_CALLSIGN_LEN - 1] = '\0';

  strncpy(grid, settings.locator, MAX_GRID_LEN - 1);
  grid[MAX_GRID_LEN - 1] = '\0';

  int powerDbm = (int) settings.dbm;

  ESP_LOGI("Scheduler", "Transmitting: %s %s %d dBm", callsign, grid, powerDbm);

  // ... rest of transmit logic ...
}