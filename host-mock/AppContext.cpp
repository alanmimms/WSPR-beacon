#include "AppContext.h"
#include "Logger.h"
#include "GPIO.h"
#include "Net.h"
#include "NVS.h"
#include "Si5351.h"
#include "FileSystem.h"
#include "Settings.h"
#include "WebServer.h"
#include "Timer.h"
#include "Task.h"
#include "EventGroup.h"

AppContext::AppContext() {
  logger = new Logger();
  gpio = new GPIO();
  net = new Net();
  nvs = new NVS();
  si5351 = new Si5351();
  fileSystem = new FileSystem();
  settings = new Settings();
  webServer = new WebServer(settings);
  timer = new Timer();
  task = new Task();
  eventGroup = new EventGroup();
}

AppContext::~AppContext() {
  delete eventGroup;
  delete task;
  delete timer;
  delete webServer;
  delete settings;
  delete fileSystem;
  delete si5351;
  delete nvs;
  delete net;
  delete gpio;
  delete logger;
}