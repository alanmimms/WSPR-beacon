#ifndef BEACON_FSM_H
#define BEACON_FSM_H

#include "WebServer.h"
#include "Scheduler.h"
#include "esp_event.h"

// Forward declarations for pointer types
class Si5351;
class Scheduler;

class BeaconFSM {
public:
  BeaconFSM();
  ~BeaconFSM();
  void run();

private:
  enum class State {
    BOOTING,
    AP_MODE,
    STA_CONNECTING,
    BEACONING,
    ERROR
  };

  State currentState;
  WebServer webServer;
  Si5351* si5351;
  Scheduler* scheduler;

  void handleBooting();
  void handleApMode();
  void handleStaConnecting();
  void handleBeaconing();

  void initHardware();
  static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
};

#endif // BEACON_FSM_H
