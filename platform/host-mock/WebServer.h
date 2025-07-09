#pragma once

#include "WebServerIntf.h"
#include "SettingsIntf.h"
#include <functional>
#include <thread>

// Forward declarations
class Scheduler;
class Beacon;

class WebServer : public WebServerIntf {
public:
  WebServer(SettingsIntf *settings);
  ~WebServer() override;

  void start() override;
  void stop() override;
  void setSettingsChangedCallback(const std::function<void()> &cb) override;
  void setScheduler(Scheduler* scheduler) override;
  void setBeacon(Beacon* beacon) override;
  void updateBeaconState(const char* networkState, const char* transmissionState, const char* band, uint32_t frequency) override;

private:
  SettingsIntf *settings;
  Scheduler* scheduler;
  Beacon* beacon;
  std::function<void()> settingsChangedCallback;
  std::thread serverThread;
  bool running;
};