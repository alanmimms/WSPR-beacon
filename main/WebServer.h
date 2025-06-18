#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"

class WebServer {
public:
  WebServer();
  void start();
  void stop();

private:
  httpd_handle_t server;

  // Handler functions are static as they are passed as C-style function pointers
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
};

#endif // WEBSERVER_H
