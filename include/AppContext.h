#pragma once

#include "LoggerIntf.h"
#include "GPIOIntf.h"
#include "NetIntf.h"
#include "NVSIntf.h"
#include "Si5351Intf.h"
#include "EventGroupIntf.h"
#include "FileSystemIntf.h"
#include "SettingsIntf.h"
#include "TaskIntf.h"
#include "TimerIntf.h"
#include "TimeIntf.h"
#include "WebServerIntf.h"

struct AppContext {
  LoggerIntf *logger;
  GPIOIntf *gpio;
  NetIntf *net;
  NVSIntf *nvs;
  Si5351Intf *si5351;
  FileSystemIntf *fileSystem;
  SettingsIntf *settings;
  WebServerIntf *webServer;
  TimerIntf *timer;
  TimeIntf *time;
  TaskIntf *task;
  EventGroupIntf *eventGroup;

  static constexpr int statusLEDGPIO = 8;

  AppContext();
  ~AppContext();
};
