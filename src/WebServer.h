#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "Settings.h"
#include <functional>

class WebServer {
public:
  WebServer(Settings *settings);
  ~WebServer();

  void start();
  void stop();

  esp_err_t getStatusJson(char *buf, size_t buflen);

  inline static const char spiffsBasePath[] = "/spiffs";

  // FSM callback registration for settings changes
  void setFsmCallback(const std::function<void()> &cb);

private:
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);

  static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename);

  httpd_handle_t server;
  Settings *settings;

  std::function<void()> fsmSettingsChangedCb;
  static WebServer *instanceForApi; // For static handler access
};

#endif // WEBSERVER_H