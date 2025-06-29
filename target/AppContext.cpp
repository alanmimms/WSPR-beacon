#include "AppContext.h"
#include "Logger.h"
#include "GPIO.h"
#include "Net.h"
#include "NVS.h"
#include "Si5351.h"
#include "FileSystem.h"
#include "Settings.h"
#include "WebServer.h"
#include "esp_event.h"
#include "esp_netif.h"

AppContext::AppContext() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  logger = new Logger();
  gpio = new GPIO();
  net = new Net();
  nvs = new NVS();
  si5351 = new Si5351();
  fileSystem = new FileSystem();
  settings = new Settings();
  webServer = new WebServer(settings);
}

AppContext::~AppContext() {
  delete webServer;
  delete settings;
  delete fileSystem;
  delete si5351;
  delete nvs;
  delete net;
  delete gpio;
  delete logger;
}