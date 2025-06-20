#include "WebServer.h"
#include "Settings.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (8192)
static const char *TAG = "WebServer";
static const char *SPIFFS_BASE_PATH = "/spiffs";

WebServer::WebServer(Settings *settings)
  : server(nullptr), settings(settings) {}

WebServer::~WebServer() {
  stop();
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

// Handler for GET /api/settings - returns all current settings as JSON
esp_err_t WebServer::apiSettingsGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self) {
    ESP_LOGE(TAG, "apiSettingsGetHandler: self is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  const cJSON *jsonObj = self->settings->getUserCJSON();
  char *jsonStr = cJSON_PrintUnformatted((cJSON *)jsonObj);
  if (!jsonStr) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonStr, strlen(jsonStr));
  cJSON_free(jsonStr);
  return ESP_OK;
}

// Handler for POST /api/settings - receives JSON and updates settings
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

  // Parse the received JSON and update settings using the API
  cJSON *root = cJSON_Parse(content);
  if (!root) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");
    return ESP_FAIL;
  }

  cJSON *item;

  item = cJSON_GetObjectItem(root, "callsign");
  if (item && cJSON_IsString(item)) {
    self->settings->setString("callsign", item->valuestring);
  }
  item = cJSON_GetObjectItem(root, "locator");
  if (item && cJSON_IsString(item)) {
    self->settings->setString("locator", item->valuestring);
  }
  item = cJSON_GetObjectItem(root, "powerDbm");
  if (item && cJSON_IsNumber(item)) {
    self->settings->setInt("powerDbm", item->valueint);
  }

  cJSON_Delete(root);

  // Persist the settings to NVS
  esp_err_t err = self->settings->storeToNVS();
  if (err == ESP_OK) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  } else {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save settings to NVS");
  }

  return ESP_OK;
}

// Handler for GET /api/status.json - returns system status as JSON
esp_err_t WebServer::apiStatusGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  if (!self) {
    ESP_LOGE(TAG, "apiStatusGetHandler: self is null");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  const cJSON *jsonObj = self->settings->getUserCJSON();
  char *jsonStr = cJSON_PrintUnformatted((cJSON *)jsonObj);
  if (!jsonStr) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jsonStr, strlen(jsonStr));
  cJSON_free(jsonStr);
  return ESP_OK;
}

// This method returns full settings JSON, identical to /api/settings
esp_err_t WebServer::getStatusJson(char *buf, size_t buflen) {
  if (!buf) return ESP_FAIL;
  const cJSON *jsonObj = settings->getUserCJSON();
  char *jsonStr = cJSON_PrintUnformatted((cJSON *)jsonObj);
  if (jsonStr && strlen(jsonStr) < buflen) {
    strcpy(buf, jsonStr);
    cJSON_free(jsonStr);
    return ESP_OK;
  }
  if (jsonStr) cJSON_free(jsonStr);
  return ESP_FAIL;
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

  // Register API handlers with user_ctx pointing to this instance
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
