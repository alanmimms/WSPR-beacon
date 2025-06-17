#pragma once

#include "Settings.h"
#include "esp_http_server.h"
#include "dns-server.h"

class WebServer {
public:
  WebServer();
  void start(Settings& settings, bool provisioningMode);
  void stop();

private:
  void mountFileSystem();
  void startDnsServer();
  void stopDnsServer();

  // HTTPD request handlers
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t settingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);
  
  // Auth helper
  static bool isAuthenticated(httpd_req_t* req);

  httpd_handle_t server;
  dns_server_handle_t dnsServer;
  
  Settings* settings;
  bool provisioningMode;
};
