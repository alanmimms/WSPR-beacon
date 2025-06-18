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
#define JSON_BUF_SIZE (1024)
static const char *TAG = "WebServer";
static const char *SPIFFS_BASE_PATH = "/spiffs";

WebServer::WebServer() : server(nullptr) {}

// Sets the Content-Type header based on the file extension
static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename) {
  if (strstr(filename, ".html")) {
    return httpd_resp_set_type(req, "text/html");
  } else if (strstr(filename, ".css")) {
    return httpd_resp_set_type(req, "text/css");
  } else if (strstr(filename, ".js")) {
    return httpd_resp_set_type(req, "application/javascript");
  } else if (strstr(filename, ".ico")) {
    return httpd_resp_set_type(req, "image/x-icon");
  }
  return httpd_resp_set_type(req, "text/plain");
}

// Handler to serve the root URL, which redirects to index.html for a clean SPA experience
esp_err_t WebServer::rootGetHandler(httpd_req_t *req) {
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/index.html");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// Generic handler to serve any static file from the SPIFFS '/spiffs' directory
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

// Handler for GET /api/settings - returns current settings as JSON
esp_err_t WebServer::apiSettingsGetHandler(httpd_req_t *req) {
  char* jsonBuffer = (char*)malloc(JSON_BUF_SIZE);
  if (!jsonBuffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  if (getSettingsJson(jsonBuffer, JSON_BUF_SIZE) == ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonBuffer, strlen(jsonBuffer));
  } else {
    ESP_LOGE(TAG, "Failed to retrieve settings as JSON");
    httpd_resp_send_500(req);
  }

  free(jsonBuffer);
  return ESP_OK;
}

// Handler for POST /api/settings - receives JSON and updates settings
esp_err_t WebServer::apiSettingsPostHandler(httpd_req_t *req) {
  char content[JSON_BUF_SIZE];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // saveSettingsJson will validate the JSON and handle saving
  esp_err_t err = saveSettingsJson(content);

  if (err == ESP_OK) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  } else {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format or save failed");
  }

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

  const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = rootGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &root);

  const httpd_uri_t apiGet = {
    .uri = "/api/settings",
    .method = HTTP_GET,
    .handler = apiSettingsGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &apiGet);

  const httpd_uri_t apiPost = {
    .uri = "/api/settings",
    .method = HTTP_POST,
    .handler = apiSettingsPostHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &apiPost);

  const httpd_uri_t fileServer = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = fileGetHandler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(server, &fileServer);
}

void WebServer::stop() {
  if (server) {
    httpd_stop(server);
    server = nullptr;
  }
}
