#pragma once

#include "WebServerIntf.h"
#include "SettingsIntf.h"
#include <functional>
#include "esp_http_server.h"

class WebServer : public WebServerIntf {
public:
  WebServer(SettingsIntf *settings);
  ~WebServer() override;

  void start() override;
  void stop() override;
  void setSettingsChangedCallback(const std::function<void()> &cb) override;

  inline static const char spiffsBasePath[] = "/spiffs";

private:
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);
  static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename);

  httpd_handle_t server;
  SettingsIntf *settings;
  std::function<void()> settingsChangedCallback;
  static WebServer *instanceForApi;
};