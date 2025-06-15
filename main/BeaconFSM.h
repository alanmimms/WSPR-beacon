#pragma once

#include "esp_event.h"
#include "esp_timer.h"
#include "si5351.h"
#include "Settings.h"
#include "WebServer.h"
#include "Scheduler.h"

// Forward declaration of the main context class
class BeaconFsm;

// --- Abstract Base Class for all States ---
class BeaconState {
public:
  BeaconState(BeaconFsm* context) : context(context) {}
  virtual ~BeaconState() {}
  virtual void enter() = 0;
  virtual void exit() {}

protected:
  BeaconFsm* context;
};

// --- Main Application Context Class ---
class BeaconFsm {
public:
  BeaconFsm();
  ~BeaconFsm();

  void start();
  void transitionTo(BeaconState* newState);
  
  // The Si5351 instance is public to be accessible for the Scheduler
  Si5351 si5351;

friend class InitialState;
friend class ConnectingState;
friend class ProvisioningState;
friend class ConnectedState;
friend class TransmittingState;

private:
  // --- Instance Variables (Reordered to match constructor) ---
  BeaconState* currentState;
  // si5351 is public, initialized via public member initializer
  Settings settings;
  WebServer webServer;
  Scheduler scheduler;
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
