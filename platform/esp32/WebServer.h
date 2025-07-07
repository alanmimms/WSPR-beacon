#pragma once

#include "WebServerIntf.h"
#include "SettingsIntf.h"
#include "TimeIntf.h"
#include "Scheduler.h"
#include <functional>
#include "esp_http_server.h"

// Function to update beacon state for status API
void updateBeaconState(const char* netState, const char* txState, const char* band, int frequency);

class WebServer : public WebServerIntf {
public:
  WebServer(SettingsIntf *settings, TimeIntf *time);
  ~WebServer() override;

  void start() override;
  void stop() override;
  void setSettingsChangedCallback(const std::function<void()> &cb) override;
  void setScheduler(Scheduler* scheduler);

  inline static const char spiffsBasePath[] = "/spiffs";

private:
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);
  static esp_err_t apiTimeGetHandler(httpd_req_t *req);
  static esp_err_t apiTimeSyncHandler(httpd_req_t *req);
  static esp_err_t apiLiveStatusHandler(httpd_req_t *req);
  static esp_err_t apiWifiScanHandler(httpd_req_t *req);
  static esp_err_t captivePortalHandler(httpd_req_t *req);
  static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename);

  httpd_handle_t server;
  SettingsIntf *settings;
  TimeIntf *time;
  Scheduler *scheduler;
  std::function<void()> settingsChangedCallback;
  static WebServer *instanceForApi;
};