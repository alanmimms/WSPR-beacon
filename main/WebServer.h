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
  // Helper methods
  void mountFileSystem();
  void startDnsServer();
  void stopDnsServer();

  // HTTPD request handlers
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t settingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req); // New API handler

  // Server instance handles
  httpd_handle_t server;
  dns_server_handle_t dnsServer;
  
  // Member variables to store context
  Settings* settings;
  bool provisioningMode;
};
