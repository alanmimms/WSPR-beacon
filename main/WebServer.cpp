#include "WebServer.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char* TAG = "WebServer";

WebServer::WebServer() : server(nullptr), settings(nullptr) {}

// Example of a simple root handler.
esp_err_t WebServer::rootGetHandler(httpd_req_t* req) {
  // Retrieve the WebServer instance from the request context.
  WebServer* self = static_cast<WebServer*>(req->user_ctx);
  
  // Now you can access settings if needed, for example:
  // const char* callsign = self->settings->getCallsign();
  
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, "<h1>ESP32 WSPR Beacon</h1><p>Provisioning Mode</p>", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

void WebServer::start(Settings& settingsRef) {
  // Store the address of the settings object for use in handlers.
  this->settings = &settingsRef;

  if (server != nullptr) {
    ESP_LOGW(TAG, "Server already running.");
    return;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192; // Increase stack size for server task.
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting web server.");
  if (httpd_start(&server, &config) == ESP_OK) {
    // Register a handler for the root URL.
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = rootGetHandler,
        .user_ctx  = this // Pass the instance pointer to the handler.
    };
    httpd_register_uri_handler(server, &root_uri);
  } else {
    ESP_LOGE(TAG, "Error starting web server!");
  }
}

void WebServer::stop() {
  if (server != nullptr) {
    httpd_stop(server);
    server = nullptr;
    ESP_LOGI(TAG, "Web server stopped.");
  }
}
