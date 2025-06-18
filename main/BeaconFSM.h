#ifndef BEACONFSM_H
#define BEACONFSM_H

#include "WebServer.h"
#include "esp_event.h"

// Forward declarations for pointer types to avoid circular dependencies
// and to only include headers where the full class definition is needed.
class Si5351;
class Scheduler;

class BeaconFSM {
public:
  BeaconFSM();
  ~BeaconFSM();

  // The main run loop for the state machine
  void run();

private:
  // Application states
  enum class State {
    BOOTING,
    AP_MODE,
    STA_CONNECTING,
    BEACONING,
    ERROR
  };

  // State handler methods
  void handleBooting();
  void handleApMode();
  void handleStaConnecting();
  void handleBeaconing();

  // Helper methods
  void initHardware();
  static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);

  State currentState;

  // Pointers to objects that will be dynamically allocated
  // after the OS and drivers are initialized.
  WebServer* webServer;
  Si5351* si5351;
  Scheduler* scheduler;
};

#endif // BEACONFSM_H
