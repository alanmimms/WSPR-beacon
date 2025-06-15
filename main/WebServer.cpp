#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

// Note: No need to include esp_netif_dns_server.h anymore
// The custom dns-server.h is included via WebServer.h

static const char* TAG = "WebServer";
static const char* SPIFFS_PARTITION_LABEL = "storage";
static const char* SPIFFS_BASE_PATH = "/spiffs";

WebServer::WebServer() : server(nullptr), dnsServer(nullptr), settings(nullptr) {}

void WebServer::mountFileSystem() {
  ESP_LOGI(TAG, "Initializing SPIFFS");

  esp_vfs_spiffs_conf_t conf = {
    .base_path = SPIFFS_BASE_PATH,
    .partition_label = SPIFFS_PARTITION_LABEL,
    .max_files = 5,
    .format_if_mount_failed = true
  };
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) ESP_LOGE(TAG, "Failed to mount or format filesystem");
    else if (ret == ESP_ERR_NOT_FOUND) ESP_LOGE(TAG, "Failed to find SPIFFS partition '%s'", SPIFFS_PARTITION_LABEL);
    else ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
  }
}

void WebServer::startDnsServer() {
  ESP_LOGI(TAG, "Starting custom DNS server for captive portal");

  // Configure the DNS server to redirect all queries to the AP's IP
  dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");

  // Start the custom DNS server
  dnsServer = start_dns_server(&config);
  if (dnsServer == NULL) {
      ESP_LOGE(TAG, "Failed to start custom DNS server");
  }
}

void WebServer::stopDnsServer() {
  if (dnsServer) {
    ESP_LOGI(TAG, "Stopping custom DNS server");
    stop_dns_server(dnsServer);
    dnsServer = nullptr;
  }
}

esp_err_t WebServer::fileGetHandler(httpd_req_t* req) {
    // With the custom DNS server, we no longer need complex redirect logic here.
    // The DNS server ensures all traffic comes to our IP. We just serve the file.
    char filepath[256];
    const char* uri = req->uri;

    // If root is requested, serve index.html. This handles the captive portal redirect.
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    size_t basePathLen = strlen(SPIFFS_BASE_PATH);
    size_t uriLen = strlen(uri);

    if (basePathLen + uriLen + 1 > sizeof(filepath)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
        return ESP_OK;
    }

    strncpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath) - 1);
    filepath[sizeof(filepath) - 1] = '\0';
    strncat(filepath, uri, sizeof(filepath) - basePathLen - 1);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    if (strstr(filepath, ".html")) httpd_resp_set_type(req, "text/html");
    else if (strstr(filepath, ".ico")) httpd_resp_set_type(req, "image/x-icon");
    else httpd_resp_set_type(req, "text/plain");

    char buffer[1024];
    int bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytesRead) != ESP_OK) {
            close(fd);
            return ESP_FAIL;
        }
    }
    close(fd);
    
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::settingsPostHandler(httpd_req_t *req) {
    WebServer* self = static_cast<WebServer*>(req->user_ctx);
    // Placeholder for handling form data to save settings via self->settings
    httpd_resp_send(req, "Settings POST received", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void WebServer::start(Settings& settingsRef, bool provisioningMode) {
  if (server != nullptr) return;
  
  this->settings = &settingsRef;

  mountFileSystem();
  
  if (provisioningMode) {
    startDnsServer();
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t fileGetUri = {.uri = "/*", .method = HTTP_GET, .handler = fileGetHandler, .user_ctx = this};
    httpd_register_uri_handler(server, &fileGetUri);

    httpd_uri_t settingsPostUri = {.uri = "/settings", .method = HTTP_POST, .handler = settingsPostHandler, .user_ctx = this};
    httpd_register_uri_handler(server, &settingsPostUri);
  } else {
    ESP_LOGE(TAG, "Error starting web server!");
  }
}

void WebServer::stop() {
  stopDnsServer();
  if (server != nullptr) {
    httpd_stop(server);
    server = nullptr;
  }
}
