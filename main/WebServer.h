#pragma once

#include "Settings.h"
#include "esp_http_server.h"
#include "dns-server.h" // Using your custom DNS server header

class WebServer {
public:
  WebServer();

  /**
   * @brief Starts the web server.
   * @param settings A reference to the application's settings object.
   * @param provisioningMode If true, start in captive portal mode.
   * If false, start in normal serving mode.
   */
  void start(Settings& settings, bool provisioningMode);

  /**
   * @brief Stops the web server and associated services (like DNS).
   */
  void stop();

private:
  // Helper methods
  void mountFileSystem();
  void startDnsServer();
  void stopDnsServer();

  // HTTPD request handlers
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t settingsPostHandler(httpd_req_t *req);

  // Server instance handles
  httpd_handle_t server;
  dns_server_handle_t dnsServer; // Use handle from your dns-server.h

  // Member variables to store context
  Settings* settings;
  bool provisioningMode;
};
