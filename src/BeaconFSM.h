#ifndef BEACONFSM_H
#define BEACONFSM_H

#include "AppContext.h"

class BeaconFSM {
public:
  BeaconFSM(AppContext *ctx);
  ~BeaconFSM();

  void run();
  void onSettingsChanged();
  void cancelTransmit();
  void syncTime();
  void periodicTimeSync();
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
  void scheduleNextTransmit();
  void startTransmit();
  void endTransmit();

  void onTxDurationTimeout();
  void onTxScheduleTimeout();

  State currentState;

  bool txCanceled;
  bool txInProgress;

  AppContext *ctx;

  TimerIntf *txDurationTimer;
  TimerIntf *txScheduleTimer;
};

#endif // BEACONFSM_H
