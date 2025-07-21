#include "WebServer.h"
#include "Beacon.h"
#include "AppContext.h"
#include "HttpHandlerIntf.h"
#include "ESP32HttpWrappers.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iomanip>
#include <sstream>
#include <memory>

// Include secrets.h if it exists - it contains WiFi credentials for testing
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (8192)
static const char *TAG = "WebServer";

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// Global beacon state for status API
static struct {
  const char* networkState = "BOOTING";
  const char* transmissionState = "IDLE";
  const char* currentBand = "20m";
  int currentFrequency = 14097100;
  bool isTransmitting = false;
} g_beaconState;
static const char *SPIFFS_BASE_PATH = "/spiffs";

WebServer *WebServer::instanceForApi = nullptr;
static std::unique_ptr<ESP32HttpEndpointHandler> g_endpointHandler = nullptr;

// Function to update beacon state from outside (legacy - calls member function)
void updateBeaconState(const char* netState, const char* txState, const char* band, int frequency) {
  if (WebServer::instanceForApi) {
    WebServer::instanceForApi->updateBeaconState(netState, txState, band, static_cast<uint32_t>(frequency));
  }
}


WebServer::WebServer(SettingsIntf *settings, TimeIntf *time)
  : server(nullptr), settings(settings), time(time), scheduler(nullptr), beacon(nullptr) {
  instanceForApi = this;
  g_endpointHandler = std::make_unique<ESP32HttpEndpointHandler>(settings, time);
}

WebServer::~WebServer() {
  stop();
}

void WebServer::setSettingsChangedCallback(const std::function<void()> &cb) {
  settingsChangedCallback = cb;
  if (g_endpointHandler) {
    g_endpointHandler->setSettingsChangedCallback(cb);
  }
}

void WebServer::setScheduler(Scheduler* sched) {
  scheduler = sched;
  if (g_endpointHandler) {
    g_endpointHandler->setScheduler(sched);
  }
}

void WebServer::setBeacon(Beacon* beaconInstance) {
  beacon = beaconInstance;
  if (g_endpointHandler) {
    g_endpointHandler->setBeacon(beaconInstance);
  }
}

void WebServer::updateBeaconState(const char* netState, const char* txState, const char* band, uint32_t frequency) {
  g_beaconState.networkState = netState;
  g_beaconState.transmissionState = txState;
  g_beaconState.currentBand = band;
  g_beaconState.currentFrequency = frequency;
  g_beaconState.isTransmitting = (strcmp(txState, "TRANSMITTING") == 0);
  
  if (g_endpointHandler) {
    g_endpointHandler->updateBeaconState(netState, txState, band, frequency);
  }
}

esp_err_t WebServer::setContentTypeFromFile(httpd_req_t *req, const char *filename) {
  if (strstr(filename, ".html")) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    return ESP_OK;
  }
  if (strstr(filename, ".css")) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    return ESP_OK;
  }
  if (strstr(filename, ".js")) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    return ESP_OK;
  }
  if (strstr(filename, ".ico")) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    return ESP_OK;
  }
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
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiSettings(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiSettingsPostHandler(httpd_req_t *req) {
  ESP_LOGI(TAG, "*** POST REQUEST RECEIVED *** apiSettingsPostHandler: Settings save requested");
  
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiSettings(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiStatusGetHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiStatus(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
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
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiTime(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiTimeSyncHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiTimeSync(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiLiveStatusHandler(httpd_req_t *req) {
  // Live status is the same as regular status for our implementation
  return apiStatusGetHandler(req);
}

esp_err_t WebServer::apiWifiScanHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiWifiScan(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
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

esp_err_t WebServer::apiCalibrationStartHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiCalibrationStart(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiCalibrationStopHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiCalibrationStop(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiCalibrationAdjustHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiCalibrationAdjust(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebServer::apiCalibrationCorrectionHandler(httpd_req_t *req) {
  if (!g_endpointHandler) {
    ESP_LOGE(TAG, "Endpoint handler not initialized");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  ESP32HttpRequest request(req);
  ESP32HttpResponse response(req);
  
  HttpHandlerResult result = g_endpointHandler->handleApiCalibrationCorrection(&request, &response);
  return (result == HttpHandlerResult::OK) ? ESP_OK : ESP_FAIL;
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

  const httpd_uri_t apiTimeSync = {
    .uri = "/api/time/sync",
    .method = HTTP_POST,
    .handler = apiTimeSyncHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiTimeSync);

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

  // Calibration API endpoints
  const httpd_uri_t apiCalibrationStart = {
    .uri = "/api/calibration/start",
    .method = HTTP_POST,
    .handler = apiCalibrationStartHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiCalibrationStart);

  const httpd_uri_t apiCalibrationStop = {
    .uri = "/api/calibration/stop",
    .method = HTTP_POST,
    .handler = apiCalibrationStopHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiCalibrationStop);

  const httpd_uri_t apiCalibrationAdjust = {
    .uri = "/api/calibration/adjust",
    .method = HTTP_POST,
    .handler = apiCalibrationAdjustHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiCalibrationAdjust);

  const httpd_uri_t apiCalibrationCorrection = {
    .uri = "/api/calibration/correction",
    .method = HTTP_POST,
    .handler = apiCalibrationCorrectionHandler,
    .user_ctx = this
  };
  httpd_register_uri_handler(server, &apiCalibrationCorrection);

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
