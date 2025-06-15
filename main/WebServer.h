#pragma once

#include "Settings.h"
#include "esp_http_server.h"

class WebServer {
public:
  WebServer();

  /**
   * @brief Starts the web server.
   * * @param settings A reference to the application's settings object.
   * The reference guarantees that a valid, non-null object is provided.
   */
  void start(Settings& settings);

  /**
   * @brief Stops the web server if it is running.
   */
  void stop();

private:
  httpd_handle_t server;
  Settings* settings; // Internally, we can store a pointer for convenience.

  // --- Request Handlers ---
  static esp_err_t rootGetHandler(httpd_req_t *req);
};
