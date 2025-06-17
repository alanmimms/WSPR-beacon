#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

static const char* TAG = "WebServer";
static const char* SPIFFS_PARTITION_LABEL = "storage";
static const char* SPIFFS_BASE_PATH = "/spiffs";

// Declare configurations as static const with camelCase names and multi-line style.
static const esp_vfs_spiffs_conf_t spiffsConfig = {
  .base_path = SPIFFS_BASE_PATH,
  .partition_label = SPIFFS_PARTITION_LABEL,
  .max_files = 50,
  .format_if_mount_failed = true
};

static const dns_server_config_t dnsConfig = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");

WebServer::WebServer() : server(nullptr), dnsServer(nullptr), settings(nullptr), provisioningMode(false) {}

void WebServer::mountFileSystem() {
  ESP_LOGI(TAG, "Initializing SPIFFS");
  esp_err_t ret = esp_vfs_spiffs_register(&spiffsConfig);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) ESP_LOGE(TAG, "Failed to mount or format filesystem");
    else if (ret == ESP_ERR_NOT_FOUND) ESP_LOGE(TAG, "Failed to find SPIFFS partition '%s'", spiffsConfig.partition_label);
    else ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
  }
}

void WebServer::startDnsServer() {
  ESP_LOGI(TAG, "Starting custom DNS server");
  dnsServer = start_dns_server(&dnsConfig);
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

bool WebServer::isAuthenticated(httpd_req_t* req) {
  WebServer* self = static_cast<WebServer*>(req->user_ctx);
  char authHeader[128] = {0};
  
  if (httpd_req_get_hdr_value_str(req, "Authorization", authHeader, sizeof(authHeader)) != ESP_OK) {
    return false;
  }

  if (strncmp(authHeader, "Basic ", 6) != 0) {
    return false;
  }

  unsigned char decodedCreds[128] = {0};
  size_t decodedLen = 0;
  const unsigned char* base64Creds = (const unsigned char*)authHeader + 6;
  
  if (mbedtls_base64_decode(decodedCreds, sizeof(decodedCreds) - 1, &decodedLen, base64Creds, strlen((const char*)base64Creds)) != 0) {
    return false;
  }

  const char* separator = strchr((const char*)decodedCreds, ':');
  if (!separator) {
    return false;
  }

  const char* storedUser = self->settings->getValue("adminUser");
  const char* storedPass = self->settings->getValue("adminPass");

  if (!storedUser || !storedPass) {
    return false;
  }
    
  size_t userLen = separator - (const char*)decodedCreds;
  bool userMatch = (strlen(storedUser) == userLen) && (strncmp((const char*)decodedCreds, storedUser, userLen) == 0);
  bool passMatch = (strcmp(separator + 1, storedPass) == 0);

  return userMatch && passMatch;
}

esp_err_t WebServer::apiStatusGetHandler(httpd_req_t* req) {
  WebServer* self = static_cast<WebServer*>(req->user_ctx);
  if (!self->provisioningMode && !isAuthenticated(req)) {
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
    return ESP_OK;
  }
    
  cJSON* root = cJSON_CreateObject();
    
  for (int i = 0; i < self->settings->numSettings; i++) {
    const auto& setting = self->settings->settings[i];
    if (strcmp(setting.key, "password") != 0 && strcmp(setting.key, "adminPass") != 0) {
      cJSON_AddStringToObject(root, setting.key, setting.value);
    }
  }
    
  esp_netif_ip_info_t ipInfo;
  char ipAddrStr[16] = "N/A";
  esp_netif_t* netif = self->provisioningMode ? esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") : esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if(netif) {
    esp_netif_get_ip_info(netif, &ipInfo);
    inet_ntoa_r(ipInfo.ip, ipAddrStr, sizeof(ipAddrStr));
  }
  cJSON_AddStringToObject(root, "ipAddress", ipAddrStr);

  const char* jsonString = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonString, strlen(jsonString));

  free((void*)jsonString);
  cJSON_Delete(root);
  return ESP_OK;
}

esp_err_t WebServer::fileGetHandler(httpd_req_t* req) {
  WebServer* self = static_cast<WebServer*>(req->user_ctx);
  const char* uri = req->uri;
    
  bool isControlPage = (strcmp(uri, "/index.html") == 0) || (strcmp(uri, "/security.html") == 0);
  if (strcmp(uri, "/") == 0 && !self->provisioningMode) {
    isControlPage = true;
  }
    
  if (strcmp(uri, "/provisioning.html") == 0) {
    isControlPage = false;
  }
    
  if (isControlPage && !isAuthenticated(req)) {
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"WSPR Beacon\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required");
    return ESP_OK;
  }

  if (self->provisioningMode) {
    bool isKnownAsset = (strcmp(uri, "/") == 0) || (strcmp(uri, "/provisioning.html") == 0) || (strcmp(uri, "/style.css") == 0) || (strcmp(uri, "/script.js") == 0) || (strcmp(uri, "/favicon.ico") == 0);
    if (!isKnownAsset) {
      ESP_LOGI(TAG, "Captive portal probe for URI '%s', redirecting.", uri);
      char redirectUrl[128];
      esp_netif_ip_info_t ipInfo;
      esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ipInfo);
      snprintf(redirectUrl, sizeof(redirectUrl), "http://%s/provisioning.html", inet_ntoa(ipInfo.ip));
      httpd_resp_set_status(req, "302 Found");
      httpd_resp_set_hdr(req, "Location", redirectUrl);
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  char filepath[256];
  const char* targetFile = uri;
  if (strcmp(uri, "/") == 0) {
    targetFile = self->provisioningMode ? "/provisioning.html" : "/index.html";
  }

  size_t basePathLen = strlen(SPIFFS_BASE_PATH);
  size_t targetFileLen = strlen(targetFile);
  if (basePathLen + targetFileLen + 1 > sizeof(filepath)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
    return ESP_OK;
  }
  strncpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath) - 1);
  filepath[sizeof(filepath) - 1] = '\0';
  strncat(filepath, targetFile, sizeof(filepath) - basePathLen - 1);

  isControlPage = (strcmp(uri, "/index.html") == 0) || 
    (strcmp(uri, "/security.html") == 0) ||
    (strcmp(uri, "/settings.html") == 0);

  int fd = open(filepath, O_RDONLY, 0);
  if (fd == -1) {
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
    httpd_resp_send_chunk(req, buffer, bytesRead);
  }
  close(fd);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebServer::settingsPostHandler(httpd_req_t *req) {
  WebServer* self = static_cast<WebServer*>(req->user_ctx);
  char buf[1024];
  int ret, remaining = req->content_len;

  if (remaining >= sizeof(buf)) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  ret = httpd_req_recv(req, buf, remaining);
  if (ret <= 0) {
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  char* saveptr;
  char* pair = strtok_r(buf, "&", &saveptr);
  while (pair != NULL) {
    char* value_saveptr;
    WebServer* self = static_cast<WebServer*>(req->user_ctx);
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) { /* ... */ return ESP_FAIL; }
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) { return ESP_FAIL; }
    buf[ret] = '\0';

    bool rebootRequired = false;
    char* saveptr;
    char* pair = strtok_r(buf, "&", &saveptr);
    while (pair != NULL) {
      char* value_saveptr;
      char* key = strtok_r(pair, "=", &value_saveptr);
      char* value = strtok_r(NULL, "", &value_saveptr);
        
      if (key && value) {
	self->settings->setValue(key, value);

	if (key && value) {
	  if (strcmp(key, "reboot") == 0 && strcmp(value, "true") == 0) {
	    rebootRequired = true;
	  } else {
	    self->settings->setValue(key, value);
	  }
	}
	pair = strtok_r(NULL, "&", &saveptr);
      }
      pair = strtok_r(NULL, "&", &saveptr);
    }
    
    self->settings->save();
    ESP_LOGI(TAG, "Settings saved. Rebooting.");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    self->settings->save();
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
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

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;
  
    if (httpd_start(&server, &config) == ESP_OK) {
      const httpd_uri_t apiStatusUri = {
	.uri       = "/api/status.json",
	.method    = HTTP_GET,
	.handler   = apiStatusGetHandler,
	.user_ctx  = this
      };
      ESP_ERROR_CHECK(httpd_register_uri_handler(server, &apiStatusUri));
    if (rebootRequired) {
        ESP_LOGI(TAG, "Settings saved. Rebooting.");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGI(TAG, "Settings saved.");
        httpd_resp_send(req, "Settings Saved", HTTPD_RESP_USE_STRLEN);
    }

      const httpd_uri_t fileGetUri = {
	.uri       = "/*",
	.method    = HTTP_GET,
	.handler   = fileGetHandler,
	.user_ctx  = this
      };
      ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fileGetUri));

      const httpd_uri_t settingsPostUri = {
	.uri       = "/settings",
	.method    = HTTP_POST,
	.handler   = settingsPostHandler,
	.user_ctx  = this
      };
      ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settingsPostUri));
    } else {
      ESP_LOGE(TAG, "Error starting web server!");
    }

    return ESP_OK;
  }

  void WebServer::stop() {
    stopDnsServer();
    if (server != nullptr) {
      httpd_stop(server);
      server = nullptr;
    }
  }
