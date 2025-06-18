# Context Package for Copilot Chat Reset

## Summary

- **Project:** WSPR Beacon for ESP32
- **Main Languages:** C++, C, JavaScript (frontend)
- **Backend:** Custom ESP32 web server
- **Frontend:** HTML/JS (not a framework)
- **Key conventions:**
  - C/C++: camelCase, no "m_" prefix, 2-space indent, space after keywords, use C strings/arrays, template param names UPPERCASE (not T), e.g., "nCall" not "n_call"
- **Key settings:** `callsign`, `locator` (Maidenhead), `powerDbm`
- **Main API endpoints:**
  - `GET /api/settings` and `POST /api/settings` (returns/updates settings as JSON)
  - `GET /api/status.json` (returns current status including settings as JSON)
- **Frontend requirements:**
  - Footer and status page must show latest settings (callsign, locator, power) immediately after settings are saved.
  - Footer must update instantly after settings save (without reload).
  - Status page must always fetch and show latest values on navigation.

## Open Issues

1. After saving settings (POST `/api/settings`), changes must be immediately reflected in `/api/status.json` and `/api/settings`.
2. Footer and status page must reflect the latest settings without requiring a page reload.
3. On some pages (like Home/index.html), settings fields display "?" after a settings change.
4. Backend and frontend must use camelCase for JSON keys and code.
5. All API endpoints must return current, not cached, values.

## Current Code Files

### main/WebServer.cpp
```cpp
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
static const char *SPIFFS_BASE_PATH = "/spiffs"

WebServer::WebServer(Settings &settings)
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
  char *jsonBuffer = (char*)malloc(JSON_BUF_SIZE);
  if (!jsonBuffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  if (self && self->settings.getJson(jsonBuffer, JSON_BUF_SIZE) == ESP_OK) {
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
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  char content[JSON_BUF_SIZE];
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

  esp_err_t err = self->settings.saveJson(content); // Save settings from POST
  if (err == ESP_OK) {
    self->settings.load(); // Immediately reload settings so all calls use the latest values
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  } else {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format or save failed");
  }

  return ESP_OK;
}

// Handler for GET /api/status.json - returns system status as JSON
esp_err_t WebServer::apiStatusGetHandler(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  char *jsonBuffer = (char*)malloc(JSON_BUF_SIZE);
  if (!jsonBuffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  if (self && self->getStatusJson(jsonBuffer, JSON_BUF_SIZE) == ESP_OK) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonBuffer, strlen(jsonBuffer));
  } else {
    ESP_LOGE(TAG, "Failed to retrieve status as JSON");
    httpd_resp_send_500(req);
  }
  free(jsonBuffer);
  return ESP_OK;
}

// This method should be a member so it can pull from this->settings
esp_err_t WebServer::getStatusJson(char *buf, size_t buflen) {
  cJSON *root = cJSON_CreateObject();
  char callsign[32] = "";
  char locator[16] = "";
  int powerDbm = 0;

  settings.getString("callsign", callsign, sizeof(callsign), "");
  settings.getString("locator", locator, sizeof(locator), "");
  settings.getInt("powerDbm", &powerDbm, 0);

  cJSON_AddStringToObject(root, "callsign", callsign);
  cJSON_AddStringToObject(root, "locator", locator);
  cJSON_AddNumberToObject(root, "power_dbm", powerDbm);

  // Add other status fields as needed (ip_address, hostname, etc)

  char *rendered = cJSON_PrintUnformatted(root);
  if (rendered && strlen(rendered) < buflen) {
    strcpy(buf, rendered);
    cJSON_free(rendered);
    cJSON_Delete(root);
    return ESP_OK;
  }
  if (rendered) cJSON_free(rendered);
  cJSON_Delete(root);
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
```

### main/WebServer.h
```cpp
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "Settings.h"

class WebServer {
public:
  WebServer(Settings &settings);
  ~WebServer();

  void start();
  void stop();

  esp_err_t getStatusJson(char *buf, size_t buflen);

private:
  static esp_err_t rootGetHandler(httpd_req_t *req);
  static esp_err_t fileGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
  static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
  static esp_err_t apiStatusGetHandler(httpd_req_t *req);

  static esp_err_t setContentTypeFromFile(httpd_req_t *req, const char *filename);

  httpd_handle_t server;
  Settings &settings;
};

#endif // WEBSERVER_H
```

### web/script.js
```javascript
document.addEventListener('DOMContentLoaded', () => {
  // --- Highlight Active Nav Link ---
  const currentPage = window.location.pathname;
  const navLinks = document.querySelectorAll('nav a');
  navLinks.forEach(link => {
    const linkPath = link.getAttribute('href');
    // Treat "/" as a request for "index.html" for highlighting purposes
    if (linkPath === currentPage || (currentPage === '/' && linkPath === '/index.html')) {
      link.classList.add('active');
    } else {
      link.classList.remove('active');
    }
  });

  // --- Fetch and Populate Footer Values ---
  async function updateFooter() {
    try {
      const response = await fetch('/api/status.json');
      if (!response.ok) throw new Error('Failed to fetch status');
      const data = await response.json();

      const callsignDiv = document.getElementById('footer-callsign');
      if (callsignDiv) {
        callsignDiv.textContent = data.callsign
          ? `Callsign: ${data.callsign}` : 'Callsign: ?';
      }

      const hostnameDiv = document.getElementById('footer-hostname');
      if (hostnameDiv) {
        hostnameDiv.textContent = data.hostname
          ? `Hostname: ${data.hostname}` : 'Hostname: ?';
      }
    } catch (error) {
      const callsignDiv = document.getElementById('footer-callsign');
      const hostnameDiv = document.getElementById('footer-hostname');
      if (callsignDiv) callsignDiv.textContent = 'Callsign: ?';
      if (hostnameDiv) hostnameDiv.textContent = 'Hostname: ?';
    }
  }
  updateFooter();

  // --- Fetch and Populate Home Page (index.html) Status Values ---
  async function updateHomeStatus() {
    try {
      const response = await fetch('/api/status.json');
      if (!response.ok) throw new Error('Failed to fetch status');
      const data = await response.json();

      // Example for status fields on index.html:
      if (document.getElementById('status-callsign')) {
        document.getElementById('status-callsign').textContent = data.callsign || '?';
      }
      if (document.getElementById('status-locator')) {
        document.getElementById('status-locator').textContent = data.locator || '?';
      }
      if (document.getElementById('status-power_dbm')) {
        document.getElementById('status-power_dbm').textContent = (data.power_dbm !== undefined) ? data.power_dbm : '?';
      }
      // Add any other dynamic status fields you display
    } catch (error) {
      if (document.getElementById('status-callsign')) {
        document.getElementById('status-callsign').textContent = '?';
      }
      if (document.getElementById('status-locator')) {
        document.getElementById('status-locator').textContent = '?';
      }
      if (document.getElementById('status-power_dbm')) {
        document.getElementById('status-power_dbm').textContent = '?';
      }
    }
  }

  // --- Provisioning Form Handler ---
  const form = document.getElementById('settings-form');
  if (form) {
    const statusMessage = document.getElementById('status-message');
    const submitBtn = document.getElementById('submit-btn');

    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      submitBtn.disabled = true;
      submitBtn.textContent = 'Saving...';
      const data = new URLSearchParams(new FormData(form));
      try {
        const response = await fetch('/api/settings', {
          method: 'POST',
          body: data,
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
          }
        });
        if (response.ok) {
          if (statusMessage) statusMessage.textContent = 'Settings saved!';
          await updateFooter(); // Immediately update footer after save
        } else {
          if (statusMessage) statusMessage.textContent = 'Failed to save settings.';
        }
      } catch (error) {
        if (statusMessage) statusMessage.textContent = 'Error saving settings.';
      } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = 'Save';
      }
    });
  }

  // --- Refresh footer when switching pages (nav click) ---
  const navLinksArr = Array.from(document.querySelectorAll('nav a'));
  navLinksArr.forEach(link => {
    link.addEventListener('click', () => {
      setTimeout(updateFooter, 250); // Give page time to load, then update footer
    });
  });

  // --- When on Home page, keep status live ---
  if (window.location.pathname.endsWith('index.html') || window.location.pathname === '/') {
    updateHomeStatus();
    // Optionally, refresh status every few seconds:
    // setInterval(updateHomeStatus, 5000);
  }
});
```

## Next Steps

- Use this context as the first message in a fresh Copilot Chat.
- Attach the above code files (WebServer.cpp, WebServer.h, script.js) or their latest versions.
- State your next specific issue or task (e.g., "Fix: sometimes status returns stale data after POST /api/settings").