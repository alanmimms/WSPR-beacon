#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "Settings.h"

class WebServer {
public:
  void start(Settings &settings);
  void stop();

  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);
  static esp_err_t apiSecurityPostHandler(httpd_req_t *req);

private:
  static httpd_handle_t server;
  Settings *settings;
};

#endif // WEBSERVER_H
