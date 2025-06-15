#pragma once

#include "esp_event.h"
#include "esp_timer.h"
#include "si5351.h"
#include "Settings.h"
#include "WebServer.h"

// Forward declaration of the main context class
class BeaconFsm;

// --- Abstract Base Class for all States ---
// Each state will inherit from this and implement its own logic.
class BeaconState {
public:
  // States are constructed with a pointer to the main FSM context.
  BeaconState(BeaconFsm* context) : context(context) {}
  virtual ~BeaconState() {}

  // Called when entering the state. Pure virtual, must be implemented.
  virtual void enter() = 0;
  // Called when exiting the state.
  virtual void exit() {} // Default empty implementation is often sufficient.

protected:
  BeaconFsm* context;
};

// --- Main Application Context Class ---
// This class owns all the components and data, and manages state transitions.
class BeaconFsm {
public:
  BeaconFsm();
  ~BeaconFsm();

  void start();
  void transitionTo(BeaconState* newState);

// Allow state classes to access private members of the FSM
friend class InitialState;
friend class ConnectingState;
friend class ProvisioningState;
friend class ConnectedState;
friend class TransmittingState;

private:
  // --- Instance Variables ---
  BeaconState* currentState;
  Si5351 si5351;
  Settings settings;
  WebServer webServer;
  esp_timer_handle_t wifiRetryTimer;
  esp_timer_handle_t provisionTimer;
  int wifiConnectAttempts;

  // --- Instance Methods for internal logic ---
  void provisionTimeout();
  void wifiRetryTimeout();
  void wifiStaGotIp(void* eventData);
  void wifiStaDisconnected(void* eventData);
  void startProvisioningMode();
  void stopWebServer();
  void connectToWifi();
  void startNtpSync();

  // --- Static callback wrappers for ESP-IDF APIs ---
  static void onProvisionTimeout(void* arg);
  static void onWifiRetryTimeout(void* arg);
  static void onWifiStaGotIp(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
  static void onWifiStaDisconnected(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
};
