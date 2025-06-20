#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "Settings.h"

class WebServer {
public:
  WebServer(Settings *settings);
  ~WebServer();

  void start();
  void stop();

  esp_err_t getStatusJson(char *buf, size_t buflen);

  inline static const char spiffsBasePath[] = "/spiffs";

private:
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);

  static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename);

  httpd_handle_t server;
  Settings *settings;
};

#endif // WEBSERVER_H
