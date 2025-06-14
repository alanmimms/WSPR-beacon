#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <cstdint>
#include <vector>

// Defines a single time-based operational window for the beacon
struct TimeScheduleEntry {
  bool enabled = false;
  uint8_t startHour = 0;
  uint8_t startMinute = 0;
  uint8_t endHour = 23;
  uint8_t endMinute = 59;
  uint8_t daysOfWeek = 0b1111111; // Bitmask for Sun,Mon,Tue,Wed,Thu,Fri,Sat
};

// Defines a single band configuration in the rotation plan
struct BandConfig {
  uint32_t frequencyHz = 14097100; // Default WSPR frequency for 20m
  uint8_t iterations = 1;          // Transmit this many times before rotating
};

// Main structure holding all user-configurable settings
struct BeaconConfig {
  // Wi-Fi and Network Settings
  char wifiSsid[32] = "";
  char wifiPassword[64] = "";
  char hostname[32] = "";

  // Beacon Identity
  char callsign[12] = "NOCALL";
  char locator[7] = "FN42";
  int8_t powerDbm = 10;

  // Beacon Operation
  bool isRunning = false;
  int skipIntervals = 0; // 0 = tx every 2 mins, 1 = tx every 4 mins, etc.
  char timeZone[48] = "GMT0";

  // Schedules and Plans (up to 5 of each for simplicity)
  TimeScheduleEntry timeSchedules[5];
  BandConfig bandPlan[5];
  uint8_t numBandsInPlan = 0;
};

// Statistics structure
struct BeaconStats {
    uint32_t totalTxMinutes[5] = {0}; // Corresponds to each band in the plan
};


// --- NVS Manager Class ---
class ConfigManager {
public:
  // Loads the entire configuration from NVS into the provided struct.
  // Returns true on success, false on failure.
  static bool loadConfig(BeaconConfig& config);

  // Saves the entire configuration struct to NVS.
  // Returns true on success, false on failure.
  static bool saveConfig(const BeaconConfig& config);

  // Load and save operational statistics
  static bool loadStats(BeaconStats& stats);
  static bool saveStats(const BeaconStats& stats);
};

#endif // CONFIG_MANAGER_H
