#ifndef BEACONFSM_H
#define BEACONFSM_H

#include "WebServer.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "Settings.h"

class Si5351;

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

  static void timerCallback(void *arg);
  void transmit();

  State currentState;
  WebServer *webServer;
  Si5351 *si5351;
  Settings *settings;
  esp_timer_handle_t timer;
};

#endif // BEACONFSM_H
