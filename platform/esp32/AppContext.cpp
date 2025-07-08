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
#include "Time.h"
#include "Task.h"
#include "EventGroup.h"
#include "esp_event.h"
#include "esp_netif.h"

AppContext::AppContext() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  logger = new Logger();
  gpio = new GPIO();
  net = new Net();
  nvs = new NVS();
  si5351 = new Si5351Wrapper(logger);  // Pass logger to Si5351Wrapper
  fileSystem = new FileSystem();
  settings = new Settings();
  time = new Time();
  webServer = new WebServer(settings, time);
  timer = new Timer();
  task = new Task();
  eventGroup = new EventGroup();
}

AppContext::~AppContext() {
  delete eventGroup;
  delete task;
  delete timer;
  delete webServer;
  delete time;
  delete settings;
  delete fileSystem;
  delete si5351;
  delete nvs;
  delete net;
  delete gpio;
  delete logger;
}