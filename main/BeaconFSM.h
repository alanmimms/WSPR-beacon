#ifndef BEACONFSM_H
#define BEACONFSM_H

#include "WebServer.h"
#include "esp_event.h"
#include "Settings.h"

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

  void handleBooting();
  void handleApMode();
  void handleStaConnecting();
  void handleBeaconing();

  void initHardware();
  static void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);

  State currentState;
  WebServer *webServer;
  Si5351 *si5351;
  Scheduler *scheduler;
  Settings *settings;
};

#endif // BEACONFSM_H