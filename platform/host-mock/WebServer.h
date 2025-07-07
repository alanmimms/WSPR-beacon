#pragma once

#include "WebServerIntf.h"
#include "SettingsIntf.h"
#include <functional>
#include <thread>

// Forward declaration
class Scheduler;

class WebServer : public WebServerIntf {
public:
  WebServer(SettingsIntf *settings);
  ~WebServer() override;

  void start() override;
  void stop() override;
  void setSettingsChangedCallback(const std::function<void()> &cb) override;
  void setScheduler(Scheduler* scheduler) override;

private:
  SettingsIntf *settings;
  Scheduler* scheduler;
  std::function<void()> settingsChangedCallback;
  std::thread serverThread;
  bool running;
};