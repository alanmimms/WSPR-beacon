#pragma once

#include "si5351.h"
#include "Settings.h"
#include "esp_timer.h"

class Scheduler {
public:
  // The constructor takes references to the shared Si5351 and Settings objects.
  Scheduler(Si5351& si5351, Settings& settings);

  void start();
  void stop();

private:
  // This method contains the core logic that is run periodically.
  void tick();

  // Static callback function for the ESP-IDF timer.
  static void onTimer(void* arg);

  // References to the shared objects from BeaconFsm.
  Si5351& si5351;
  Settings& settings;

  // Handle for the periodic timer that drives the scheduler.
  esp_timer_handle_t timer;

  // Member variables for tracking the current transmission state.
  int currentBandIndex;
  int transmissionsOnCurrentBand;
  int skipIntervalCount;
};
