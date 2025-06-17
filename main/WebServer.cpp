#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

static const char* TAG = "WebServer";
static const char* SPIFFS_PARTITION_LABEL = "storage";
static const char* SPIFFS_BASE_PATH = "/spiffs";

// Declare the SPIFFS configuration as a static const to avoid runtime overhead.
static const esp_vfs_spiffs_conf_t spiffs_conf = {
  .base_path = SPIFFS_BASE_PATH,
  .partition_label = SPIFFS_PARTITION_LABEL,
  .max_files = 5,
  .format_if_mount_failed = true
};

WebServer::WebServer() : server(nullptr), dnsServer(nullptr), settings(nullptr), provisioningMode(false) {}

void WebServer::mountFileSystem() {
  ESP_LOGI(TAG, "Initializing SPIFFS");
  esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) ESP_LOGE(TAG, "Failed to mount or format filesystem");
    else if (ret == ESP_ERR_NOT_FOUND) ESP_LOGE(TAG, "Failed to find SPIFFS partition '%s'", spiffs_conf.partition_label);
    else ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    return;
  }
}

void WebServer::startDnsServer() {
  ESP_LOGI(TAG, "Starting custom DNS server for captive portal");
  dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
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
    WebServer* self = static_cast<WebServer*>(req->user_ctx);
    const char* uri = req->uri;

    ESP_LOGI(TAG, "GET %s", uri);

    if (self->provisioningMode) {
        bool isKnownAsset = (strcmp(uri, "/") == 0) || (strcmp(uri, "/index.html") == 0) || (strcmp(uri, "/style.css") == 0) || (strcmp(uri, "/script.js") == 0) || (strcmp(uri, "/favicon.ico") == 0);
        if (!isKnownAsset) {
            ESP_LOGI(TAG, "Captive portal probe for URI: %s. Redirecting.", uri);
            esp_netif_ip_info_t ipInfo;
            ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ipInfo));
            char ipAddrStr[16];
            inet_ntoa_r(ipInfo.ip, ipAddrStr, sizeof(ipAddrStr));
            char redirectUrl[128];
            snprintf(redirectUrl, sizeof(redirectUrl), "http://%s/index.html", ipAddrStr);
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", redirectUrl);
            httpd_resp_send(req, NULL, 0);
	    ESP_LOGI(TAG, "GET redirecting with 302 to %s", redirectUrl);
            return ESP_OK;
        }
    }

    char filepath[256];
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    if (strlen(SPIFFS_BASE_PATH) + strlen(uri) + 1 > sizeof(filepath)) {
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
      return ESP_OK;
    }
    strncpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath) - 1);
    filepath[sizeof(filepath) - 1] = '\0';
    strncat(filepath, uri, sizeof(filepath) - strlen(SPIFFS_BASE_PATH) - 1);

    ESP_LOGI(TAG, "GET opening %s", filepath);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "404 Not Found: Could not open file: %s", filepath);
        DIR *dir = opendir(SPIFFS_BASE_PATH);
        if (dir) {
            struct dirent *ent;
            ESP_LOGI(TAG, "Contents of %s:", SPIFFS_BASE_PATH);
            while ((ent = readdir(dir)) != NULL) {
                ESP_LOGI(TAG, "  - %s", ent->d_name);
            }
            closedir(dir);
        } else {
            ESP_LOGE(TAG, "Could not open directory %s", SPIFFS_BASE_PATH);
        }
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    
    if (strstr(filepath, ".html")) httpd_resp_set_type(req, "text/html");
    else if (strstr(filepath, ".ico")) httpd_resp_set_type(req, "image/x-icon");
    else if (strstr(filepath, ".css")) httpd_resp_set_type(req, "text/css");
    else if (strstr(filepath, ".js")) httpd_resp_set_type(req, "application/javascript");
    else httpd_resp_set_type(req, "text/plain");

    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
      if (httpd_resp_send_chunk(req, buffer, bytesRead) != ESP_OK) {
        close(fd);
        return ESP_FAIL;
      }
    }
    close(fd);
    ESP_LOGI(TAG, "GET finished sending %s", filepath);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::settingsPostHandler(httpd_req_t *req) {
    // Implementation to handle form POST
    return ESP_OK;
}

void WebServer::start(Settings& settingsRef, bool provMode) {
  if (server != nullptr) return;
  
  this->settings = &settingsRef;
  this->provisioningMode = provMode;

  mountFileSystem();
  
  if (this->provisioningMode) {
    startDnsServer();
  }

  // Use the default configuration which includes httpd_uri_match_wildcard
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 8192;

  if (httpd_start(&server, &config) == ESP_OK) {
    // A single wildcard handler for all GET requests is the robust solution.
    httpd_uri_t fileGetUri = {.uri = "/*", .method = HTTP_GET, .handler = fileGetHandler, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fileGetUri));

    httpd_uri_t settingsPostUri = {.uri = "/settings", .method = HTTP_POST, .handler = settingsPostHandler, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settingsPostUri));
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
