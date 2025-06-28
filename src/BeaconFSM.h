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

  // Called by WebServer when settings are changed
  void onSettingsChanged();

  // For testing or external control, allow canceling TX
  void cancelTransmit();

  // SNTP/time sync support
  void syncTime();
  void periodicTimeSync();

  // Returns true if allowed to transmit at 'now'
  bool canTransmitNow();

private:
  enum class State {
    BOOTING,
    AP_MODE,
    STA_CONNECTING,
    BEACONING,
    TX_START,
    TX_END,
    ERROR
  };

  void handleBooting();
  void handleApMode();
  void handleStaConnecting();
  void handleBeaconing();
  void handleTxStart();
  void handleTxEnd();

  void initHardware();
  static void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);

  static void txDurationTimerCallback(void *arg);
  static void txScheduleTimerCallback(void *arg);

  void startTransmit();
  void endTransmit();
  void scheduleNextTransmit();

  State currentState;
  WebServer *webServer;
  Si5351 *si5351;
  Settings *settings;

  esp_timer_handle_t txDurationTimer;
  esp_timer_handle_t txScheduleTimer;

  bool txCanceled;
  bool txInProgress;
};

#endif // BEACONFSM_H