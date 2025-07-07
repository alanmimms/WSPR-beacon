#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iomanip>
#include <sstream>

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (8192)
static const char *TAG = "WebServer";
static const char *SPIFFS_BASE_PATH = "/spiffs";

WebServer *WebServer::instanceForApi = nullptr;

WebServer::WebServer(SettingsIntf *settings, TimeIntf *time)
  : server(nullptr), settings(settings), time(time) {
  instanceForApi = this;
}

WebServer::~WebServer() {
  stop();
}

void WebServer::setSettingsChangedCallback(const std::function<void()> &cb) {
  settingsChangedCallback = cb;
}

esp_err_t WebServer::setContentTypeFromFile(httpd_req_t *req, const char *filename) {
  if (strstr(filename, ".html")) return httpd_resp_set_type(req, "text/html");
  if (strstr(filename, ".css")) return httpd_resp_set_type(req, "text/css");
  if (strstr(filename, ".js")) return httpd_resp_set_type(req, "application/javascript");
  if (strstr(filename, ".ico")) return httpd_resp_set_type(req, "image/x-icon");
  return httpd_resp_set_type(req, "text/plain");
}

esp_err_t WebServer::rootGetHandler(httpd_req_t *req) {
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/index.html");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebServer::fileGetHandler(httpd_req_t *req) {
  char filepath[FILE_PATH_MAX];
  strncpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath) - 1);
  filepath[sizeof(filepath) - 1] = '\0';
  strncat(filepath, req->uri, sizeof(filepath) - strlen(filepath) - 1);
  ESP_LOGI(TAG, "Serving file: %s", filepath);

  struct stat fileStat;
  if (stat(filepath, &fileStat) == -1 || !S_ISREG(fileStat.st_mode)) {
    ESP_LOGE(TAG, "File not found or not a regular file: %s", filepath);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  int fd = open(filepath, O_RDONLY, 0);
  if (fd == -1) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  setContentTypeFromFile(req, filepath);

  char *chunk = (char*)malloc(SCRATCH_BUFSIZE);
  if (!chunk) {
    ESP_LOGE(TAG, "Failed to allocate memory for file chunk");
    close(fd);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  ssize_t readBytes;
  do {
    readBytes = read(fd, chunk, SCRATCH_BUFSIZE);
    if (readBytes > 0) {
      if (httpd_resp_send_chunk(req, chunk, readBytes) != ESP_OK) {
        close(fd);
        free(chunk);
        ESP_LOGE(TAG, "File sending failed!");
        return ESP_FAIL;
      }
    }
  } while (readBytes > 0);

  close(fd);
  free(chunk);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebServer::apiSettingsGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self) {
    ESP_LOGE(TAG, "apiSettingsGetHandler: self is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  char *jsonStr = self->settings->toJsonString();
  if (!jsonStr) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonStr, strlen(jsonStr));
  free(jsonStr);
  return ESP_OK;
}

esp_err_t WebServer::apiSettingsPostHandler(httpd_req_t *req) {
  ESP_LOGI(TAG, "*** POST REQUEST RECEIVED *** apiSettingsPostHandler: Settings save requested");
  ESP_LOGI(TAG, "Request URI: %s", req->uri);
  ESP_LOGI(TAG, "Request method: %d", req->method);
  ESP_LOGI(TAG, "Content length: %d", req->content_len);
  
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  char *content = (char*)malloc(4096);  // Use heap instead of stack
  if (!content) {
    ESP_LOGE(TAG, "Failed to allocate memory for request content");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  int ret = httpd_req_recv(req, content, 4095);
  if (ret <= 0) {
    ESP_LOGE(TAG, "apiSettingsPostHandler: Failed to receive request body");
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
    free(content);
    return ESP_FAIL;
  }
  content[ret] = '\0';
  
  ESP_LOGI(TAG, "apiSettingsPostHandler: Received JSON (%d bytes): %s", ret, content);

  if (!self || !self->settings) {
    ESP_LOGE(TAG, "apiSettingsPostHandler: Settings instance not initialized");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Settings instance not initialized");
    free(content);
    return ESP_FAIL;
  }

  if (!self->settings->fromJsonString(content)) {
    ESP_LOGE(TAG, "apiSettingsPostHandler: Failed to parse JSON");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format or save failed");
    free(content);
    return ESP_FAIL;
  }

  if (self->settings->store()) {
    ESP_LOGI(TAG, "apiSettingsPostHandler: Settings saved successfully to NVS");
    if (self->settingsChangedCallback) {
      ESP_LOGI(TAG, "apiSettingsPostHandler: Calling settings changed callback");
      self->settingsChangedCallback();
    }
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    free(content);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "apiSettingsPostHandler: Failed to save settings to NVS");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save settings to NVS");
    free(content);
    return ESP_FAIL;
  }
}

esp_err_t WebServer::apiStatusGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self) {
    ESP_LOGE(TAG, "apiStatusGetHandler: self is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  char *jsonStr = self->settings->toJsonString();
  if (!jsonStr) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonStr, strlen(jsonStr));
  free(jsonStr);
  return ESP_OK;
}

std::string formatTimeISO(int64_t unixTime) {
  std::time_t time = static_cast<std::time_t>(unixTime);
  struct tm utc_tm;
  
  if (gmtime_r(&time, &utc_tm) == nullptr) {
    return "1970-01-01T00:00:00Z";
  }

  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

esp_err_t WebServer::apiTimeGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self || !self->time) {
    ESP_LOGE(TAG, "apiTimeGetHandler: self or time interface is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  int64_t currentTime = self->time->getTime();
  std::string isoTime = formatTimeISO(currentTime);
  bool synced = self->time->isTimeSynced();
  
  // Build JSON response
  char jsonResponse[256];
  snprintf(jsonResponse, sizeof(jsonResponse),
           "{\"unixTime\":%lld,\"isoTime\":\"%s\",\"synced\":%s}",
           (long long)currentTime, isoTime.c_str(), synced ? "true" : "false");
  
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonResponse, strlen(jsonResponse));
  return ESP_OK;
}

esp_err_t WebServer::apiLiveStatusHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self) {
    ESP_LOGE(TAG, "apiLiveStatusHandler: self is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  // Build live status JSON with basic system info
  char jsonResponse[512];
  int64_t currentTime = self->time ? self->time->getTime() : 0;
  bool synced = self->time ? self->time->isTimeSynced() : false;
  
  snprintf(jsonResponse, sizeof(jsonResponse),
           "{\"time\":%lld,\"synced\":%s,\"state\":\"IDLE\",\"uptime\":%d}",
           (long long)currentTime, synced ? "true" : "false", (int)(currentTime % 86400));
  
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonResponse, strlen(jsonResponse));
  return ESP_OK;
}

esp_err_t WebServer::apiWifiScanHandler(httpd_req_t *req) {
  ESP_LOGI(TAG, "WiFi scan requested");
  
  // Switch to APSTA mode to allow scanning
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  if (mode == WIFI_MODE_AP) {
    ESP_LOGI(TAG, "Switching to APSTA mode for scanning");
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to switch to APSTA mode: %s", esp_err_to_name(err));
      const char* errorJson = "{\"error\":\"Mode switch failed\"}";
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, errorJson, strlen(errorJson));
      return ESP_OK;
    }
  }
  
  // Perform WiFi scan
  wifi_scan_config_t scan_config = {};
  scan_config.ssid = NULL;
  scan_config.bssid = NULL;
  scan_config.channel = 0;
  scan_config.show_hidden = true;
  scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  
  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
    // Restore AP mode if scan failed
    if (mode == WIFI_MODE_AP) {
      esp_wifi_set_mode(WIFI_MODE_AP);
    }
    const char* errorJson = "{\"error\":\"Scan failed\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, errorJson, strlen(errorJson));
    return ESP_OK;
  }
  
  uint16_t ap_count = 0;
  esp_wifi_scan_get_ap_num(&ap_count);
  
  wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
  if (!ap_list) {
    const char* errorJson = "{\"error\":\"Memory allocation failed\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, errorJson, strlen(errorJson));
    return ESP_OK;
  }
  
  esp_wifi_scan_get_ap_records(&ap_count, ap_list);
  
  // Build JSON response
  char *json_response = (char *)malloc(4096);
  if (!json_response) {
    free(ap_list);
    const char* errorJson = "{\"error\":\"Memory allocation failed\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, errorJson, strlen(errorJson));
    return ESP_OK;
  }
  
  strcpy(json_response, "[");
  for (int i = 0; i < ap_count && i < 20; i++) {  // Limit to 20 APs
    if (strlen((char*)ap_list[i].ssid) == 0) {
      continue; // Skip hidden SSIDs
    }
    
    // Escape quotes in SSID
    char escaped_ssid[64];
    int j = 0, k = 0;
    while (ap_list[i].ssid[j] && k < 62) {
      if (ap_list[i].ssid[j] == '"' || ap_list[i].ssid[j] == '\\') {
        escaped_ssid[k++] = '\\';
      }
      escaped_ssid[k++] = ap_list[i].ssid[j++];
    }
    escaped_ssid[k] = '\0';
    
    char ap_json[256];
    snprintf(ap_json, sizeof(ap_json), 
             "%s{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
             strlen(json_response) > 1 ? "," : "",
             escaped_ssid,
             ap_list[i].rssi,
             ap_list[i].authmode);
    strcat(json_response, ap_json);
  }
  strcat(json_response, "]");
  
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_response, strlen(json_response));
  
  ESP_LOGI(TAG, "WiFi scan completed, found %d APs", ap_count);
  
  free(ap_list);
  free(json_response);
  
  // Restore original mode if needed
  if (mode == WIFI_MODE_AP) {
    esp_wifi_set_mode(WIFI_MODE_AP);
  }
  
  return ESP_OK;
}

esp_err_t WebServer::captivePortalHandler(httpd_req_t *req) {
  ESP_LOGI(TAG, "Captive portal detection: %s", req->uri);
  
  // For generate_204 and gen_204, return 204 No Content
  if (strstr(req->uri, "204") != NULL) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  } else {
    // For other captive portal detection URLs, redirect to home page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/index.html");
    httpd_resp_send(req, NULL, 0);
  }
  return ESP_OK;
}

void WebServer::start() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  
  // Re-enable wildcard matching now that we have enough handler slots
  config.uri_match_fn = httpd_uri_match_wildcard;
  
  // Increase max URI handlers from default (8) to accommodate all our routes
  config.max_uri_handlers = 20;

  ESP_LOGI(TAG, "Starting web server");
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start web server");
    return;
  }

  // Register root handler FIRST
  const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = rootGetHandler,
    .user_ctx = nullptr
  };
  esp_err_t ret = httpd_register_uri_handler(server, &root);
  ESP_LOGI(TAG, "Root handler registration: %s", esp_err_to_name(ret));

  const httpd_uri_t apiStatus = {
    .uri = "/api/status.json",
    .method = HTTP_GET,
    .handler = apiStatusGetHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiStatus);

  const httpd_uri_t apiPost = {
    .uri = "/api/settings",
    .method = HTTP_POST,
    .handler = apiSettingsPostHandler,
    .user_ctx = this
  };
  esp_err_t post_ret = httpd_register_uri_handler(server, &apiPost);
  ESP_LOGI(TAG, "POST /api/settings handler registration: %s", esp_err_to_name(post_ret));

  const httpd_uri_t apiGet = {
    .uri = "/api/settings",
    .method = HTTP_GET,
    .handler = apiSettingsGetHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiGet);

  const httpd_uri_t apiTime = {
    .uri = "/api/time",
    .method = HTTP_GET,
    .handler = apiTimeGetHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiTime);

  const httpd_uri_t apiLiveStatus = {
    .uri = "/api/live-status",
    .method = HTTP_GET,
    .handler = apiLiveStatusHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiLiveStatus);

  const httpd_uri_t apiWifiScan = {
    .uri = "/api/wifi/scan",
    .method = HTTP_GET,
    .handler = apiWifiScanHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiWifiScan);

  const httpd_uri_t captivePortal1 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = captivePortalHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &captivePortal1);

  const httpd_uri_t captivePortal2 = {
    .uri = "/gen_204",
    .method = HTTP_GET,
    .handler = captivePortalHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &captivePortal2);
  
  // Add more captive portal detection endpoints for better compatibility
  const httpd_uri_t captivePortal3 = {
    .uri = "/hotspot-detect.html",
    .method = HTTP_GET,
    .handler = captivePortalHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &captivePortal3);
  
  const httpd_uri_t captivePortal4 = {
    .uri = "/library/test/success.html",
    .method = HTTP_GET,
    .handler = captivePortalHandler,
    .user_ctx = this
  };
  ret = httpd_register_uri_handler(server, &captivePortal4);
  ESP_LOGI(TAG, "Captive portal handler registration: %s", esp_err_to_name(ret));

  // Register wildcard handler LAST
  const httpd_uri_t fileServer = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = fileGetHandler,
    .user_ctx = nullptr
  };
  ret = httpd_register_uri_handler(server, &fileServer);
  ESP_LOGI(TAG, "File server handler registration: %s", esp_err_to_name(ret));
}

void WebServer::stop() {
  if (server) {
    httpd_stop(server);
    server = nullptr;
  }
}