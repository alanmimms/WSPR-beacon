#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
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
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  char content[1024];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  if (!self) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Settings instance not initialized");
    return ESP_FAIL;
  }

  if (!self->settings->fromJsonString(content)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format or save failed");
    return ESP_FAIL;
  }

  if (self->settings->store()) {
    if (self->settingsChangedCallback) self->settingsChangedCallback();
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  } else {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save settings to NVS");
  }

  return ESP_OK;
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

void WebServer::start() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting web server");
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start web server");
    return;
  }

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
  httpd_register_uri_handler(server, &apiPost);

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

  const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = rootGetHandler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(server, &root);

  const httpd_uri_t fileServer = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = fileGetHandler,
    .user_ctx = nullptr
  };
  httpd_register_uri_handler(server, &fileServer);
}

void WebServer::stop() {
  if (server) {
    httpd_stop(server);
    server = nullptr;
  }
}