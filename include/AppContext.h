#pragma once

#include "LoggerIntf.h"
#include "GPIOIntf.h"
#include "NetIntf.h"
#include "NVSIntf.h"
#include "Si5351Intf.h"
#include "FileSystemIntf.h"
#include "SettingsIntf.h"
#include "WebServerIntf.h"
// Add TimerIntf, TaskIntf, EventGroupIntf as you have them

struct AppContext {
  LoggerIntf *logger;
  GPIOIntf *gpio;
  NetIntf *net;
  NVSIntf *nvs;
  Si5351Intf *si5351;
  FileSystemIntf *fileSystem;
  SettingsIntf *settings;
  WebServerIntf *webServer;
  // Add TimerIntf *timer, TaskIntf *task, EventGroupIntf *eventGroup as needed

  AppContext();
  ~AppContext();
};