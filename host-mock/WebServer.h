#pragma once

#include "WebServerIntf.h"
#include "SettingsIntf.h"
#include <functional>
#include <thread>

class WebServer : public WebServerIntf {
public:
  WebServer(SettingsIntf *settings);
  ~WebServer() override;

  void start() override;
  void stop() override;
  void setSettingsChangedCallback(const std::function<void()> &cb) override;

private:
  SettingsIntf *settings;
  std::function<void()> settingsChangedCallback;
  std::thread serverThread;
  bool running;
};