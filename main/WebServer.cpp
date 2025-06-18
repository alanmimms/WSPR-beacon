#include "WebServer.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include <string>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <dirent.h>

static const char *TAG = "WebServer";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (1024)
#define SPIFFS_BASE_PATH "/spiffs"

httpd_handle_t WebServer::server = nullptr;

static esp_err_t send_file(httpd_req_t *req, const char *filepath) {
  FILE *file = fopen(filepath, "r");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file : %s", filepath);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  char chunk[SCRATCH_BUFSIZE];
  size_t chunksize;
  do {
    chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, file);
    if (chunksize > 0) {
      if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
	fclose(file);
	ESP_LOGE(TAG, "File sending failed!");
	httpd_resp_sendstr_chunk(req, NULL);
	httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
	return ESP_FAIL;
      }
    }
  } while (chunksize != 0);

  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t WebServer::fileGetHandler(httpd_req_t *req) {
  char filepath[FILE_PATH_MAX];
  const char *uri = req->uri;

  if (strcmp(uri, "/") == 0) {
    uri = "/index.html";
  }

  strncpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath) - 1);
  filepath[sizeof(filepath) - 1] = '\0';
  strncat(filepath, uri, sizeof(filepath) - strlen(filepath) - 1);

  return send_file(req, filepath);
}

esp_err_t WebServer::apiStatusGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  cJSON *root = cJSON_CreateObject();

  cJSON_AddStringToObject(root, "callsign", self->settings->getValue("callsign"));
  cJSON_AddStringToObject(root, "locator", self->settings->getValue("locator"));
  double powerWatts = std::stod(self->settings->getValue("power"));
  cJSON_AddNumberToObject(root, "power", powerWatts);
  cJSON_AddNumberToObject(root, "power_dbm", 10 * log10(powerWatts) + 30);
  cJSON_AddNumberToObject(root, "tx_percentage", std::stoi(self->settings->getValue("tx_percentage")));
  cJSON_AddNumberToObject(root, "tx_skip", std::stoi(self->settings->getValue("tx_skip")));

  char hostname[64] = "unknown";
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  const char *hn;
  if (netif && esp_netif_get_hostname(netif, &hn) == ESP_OK) {
    strncpy(hostname, hn, sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = '\0';
  }
  cJSON_AddStringToObject(root, "hostname", hostname);

  char *json = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json, strlen(json));
  cJSON_Delete(root);
  free(json);
  return ESP_OK;
}

esp_err_t WebServer::apiSecurityPostHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  char buf[256];
  int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (len <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
  }
  buf[len] = '\0';

  cJSON *json = cJSON_Parse(buf);
  if (!json) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
  }

  cJSON *user = cJSON_GetObjectItem(json, "username");
  cJSON *pass = cJSON_GetObjectItem(json, "password");

  if (cJSON_IsString(user) && cJSON_IsString(pass)) {
    ESP_LOGI(TAG, "Received security update: user='%s'", user->valuestring);
    self->settings->setValue("username", user->valuestring);
    self->settings->setValue("password", pass->valuestring);
  }

  cJSON_Delete(json);
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

void WebServer::start(Settings &settings) {
  this->settings = &settings;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 8192;

  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed");
    return;
  }

  httpd_uri_t status_get_handler = {
    .uri = "/api/status.json",
    .method = HTTP_GET,
    .handler = WebServer::apiStatusGetHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &status_get_handler);

  httpd_uri_t security_post_handler = {
    .uri = "/api/security",
    .method = HTTP_POST,
    .handler = WebServer::apiSecurityPostHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &security_post_handler);

  httpd_uri_t wildcard_handler = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = WebServer::fileGetHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &wildcard_handler);

  // List SPIFFS files
  ESP_LOGI(TAG, "List of files in /spiffs");
  DIR *dir = opendir(SPIFFS_BASE_PATH);
  if (dir) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      char fullpath[FILE_PATH_MAX];
      strncpy(fullpath, SPIFFS_BASE_PATH, sizeof(fullpath) - 1);
      fullpath[sizeof(fullpath) - 1] = '\0';
      strncat(fullpath, "/", sizeof(fullpath) - strlen(fullpath) - 1);
      strncat(fullpath, entry->d_name, sizeof(fullpath) - strlen(fullpath) - 1);

      FILE *fp = fopen(fullpath, "r");
      if (fp) {
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fclose(fp);
	ESP_LOGI(TAG, "File: %s (%ld bytes)", entry->d_name, size);
      } else {
	ESP_LOGW(TAG, "Unable to open file: %s", entry->d_name);
      }
    }
    closedir(dir);
  } else {
    ESP_LOGE(TAG, "Failed to open SPIFFS directory");
  }

  ESP_LOGI(TAG, "Web server started");
}

void WebServer::stop() {
  if (server) {
    httpd_stop(server);
    server = nullptr;
    ESP_LOGI(TAG, "Web server stopped");
  }
}
