#include "test-server.h"
#include "Time.h"
#include "../../include/BeaconLogger.h"
#include "JTEncode.h"
#include "cJSON.h"
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <limits.h>
#include <iomanip>
#include <chrono>
#include <mutex>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

namespace fs = std::filesystem;

// Global variables

static std::atomic<bool> running{true};
static Time timeInterface;
static double g_timeScale = 1.0;
static std::chrono::steady_clock::time_point g_serverStartTime;
static int64_t g_mockStartTime;
static std::unique_ptr<BeaconLogger> g_logger;

// Get current mock time based on time scale
static int64_t getMockTime() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_serverStartTime).count();
  int64_t scaledElapsed = static_cast<int64_t>(elapsed * g_timeScale);
  return g_mockStartTime + (scaledElapsed / 1000);  // Convert to seconds
}

static std::string readFile(const std::string &path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.good()) return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string formatTimeISO(int64_t unixTime) {
  std::time_t time = static_cast<std::time_t>(unixTime);
  struct tm utc_tm;
  
#ifdef _WIN32
  if (gmtime_s(&utc_tm, &time) != 0) {
    return "1970-01-01T00:00:00Z";
  }
#else
  if (gmtime_r(&time, &utc_tm) == nullptr) {
    return "1970-01-01T00:00:00Z";
  }
#endif

  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

static cJSON* settings = nullptr;

static void initializeDefaultSettings() {
  if (settings) {
    cJSON_Delete(settings);
  }
  
  settings = cJSON_CreateObject();
  cJSON_AddStringToObject(settings, "call", "N0CALL");
  cJSON_AddStringToObject(settings, "loc", "AA00aa");
  cJSON_AddNumberToObject(settings, "pwr", 23);
  cJSON_AddNumberToObject(settings, "txPct", 20);
  cJSON_AddStringToObject(settings, "bandMode", "sequential");
  cJSON_AddStringToObject(settings, "wifiMode", "sta");
  cJSON_AddStringToObject(settings, "ssid", "");
  cJSON_AddStringToObject(settings, "pwd", "");
  cJSON_AddStringToObject(settings, "ssidAp", "WSPR-Beacon");
  cJSON_AddStringToObject(settings, "pwdAp", "wspr2024");
  cJSON_AddStringToObject(settings, "host", "wspr-beacon");
  
  cJSON* bands = cJSON_CreateObject();
  
  const char* bandNames[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
  const int bandFreqs[] = {1838100, 3570100, 7040100, 10140200, 14097100, 18106100, 21096100, 24926100, 28126100, 50293000, 144489000};
  
  for (int i = 0; i < 11; i++) {
    cJSON* band = cJSON_CreateObject();
    cJSON_AddBoolToObject(band, "en", false);
    cJSON_AddNumberToObject(band, "freq", bandFreqs[i]);
    cJSON_AddNumberToObject(band, "sched", 16777215);
    cJSON_AddItemToObject(bands, bandNames[i], band);
  }
  
  cJSON_AddItemToObject(settings, "bands", bands);
}

static cJSON* status = nullptr;

static bool loadMockData(const std::string& mockDataFile) {
  try {
    g_logger->logSystemVerbose("Attempting to load mock data", "file=" + mockDataFile, "");
    std::string mockDataContent = readFile(mockDataFile);
    if (mockDataContent.empty()) {
      g_logger->logBasic("ERROR", "Mock data file not found or empty", "file=" + mockDataFile);
      return false;
    }
    
    cJSON* mockData = cJSON_Parse(mockDataContent.c_str());
    if (!mockData) {
      g_logger->logBasic("ERROR", "Mock data JSON parsing failed", "file=" + mockDataFile);
      return false;
    }
    
    // Delete existing status and create new one from mock data
    if (status) {
      cJSON_Delete(status);
    }
    status = cJSON_Duplicate(mockData, true);
    
    // Also update settings with relevant fields from mock data
    int settingsUpdated = 0;
    std::vector<std::string> updatedFields;
    
    auto updateField = [&](const std::string& field) {
      cJSON* item = cJSON_GetObjectItem(mockData, field.c_str());
      if (item) {
        cJSON* existing = cJSON_GetObjectItem(settings, field.c_str());
        if (existing) {
          cJSON_DeleteItemFromObject(settings, field.c_str());
        }
        cJSON_AddItemToObject(settings, field.c_str(), cJSON_Duplicate(item, true));
        updatedFields.push_back(field);
        settingsUpdated++;
      }
    };
    
    updateField("call");
    updateField("loc");
    updateField("pwr");
    updateField("txPct");
    updateField("host");
    updateField("wifiMode");
    updateField("ssid");
    updateField("ssidAp");
    updateField("pwdAp");
    updateField("bandMode");
    
    std::string fieldList = "";
    for (size_t i = 0; i < updatedFields.size(); ++i) {
      if (i > 0) fieldList += ", ";
      fieldList += updatedFields[i];
    }
    g_logger->logSystemEvent("Settings updated from mock data", "fields_updated=" + std::to_string(settingsUpdated) + ", fields=[" + fieldList + "]");
    
    // Always set resetTime to current startup time
    std::string resetTime = formatTimeISO(timeInterface.getStartTime());
    cJSON_DeleteItemFromObject(status, "resetTime");
    cJSON_AddStringToObject(status, "resetTime", resetTime.c_str());
    
    g_logger->logSystemEvent("Mock data loaded successfully", "file=" + mockDataFile + ", size=" + std::to_string(mockDataContent.size()) + " bytes");
    
    cJSON_Delete(mockData);
    return true;
  } catch (const std::exception& e) {
    g_logger->logBasic("ERROR", "Mock data parsing failed", "file=" + mockDataFile + ", error=" + std::string(e.what()));
    return false;
  }
}

static void initializeDefaultStatus() {
  // Fallback default status if mock data loading fails
  if (status) {
    cJSON_Delete(status);
  }
  
  status = cJSON_CreateObject();
  cJSON_AddStringToObject(status, "call", "N0CALL");
  cJSON_AddStringToObject(status, "loc", "AA00aa");
  cJSON_AddNumberToObject(status, "pwr", 23);
  cJSON_AddNumberToObject(status, "txPct", 20);
  cJSON_AddStringToObject(status, "host", "wspr-beacon");
  cJSON_AddStringToObject(status, "curBand", "20m");
  
  std::string resetTime = formatTimeISO(timeInterface.getStartTime());
  cJSON_AddStringToObject(status, "resetTime", resetTime.c_str());
  
  cJSON_AddStringToObject(status, "ssid", "TestWiFi");
  cJSON_AddNumberToObject(status, "rssi", -70);
  cJSON_AddStringToObject(status, "netState", "READY");
  
  // Create stats object
  cJSON* stats = cJSON_CreateObject();
  cJSON_AddNumberToObject(stats, "txCnt", 0);
  cJSON_AddNumberToObject(stats, "txMin", 0);
  
  // Create bands stats object
  cJSON* bands = cJSON_CreateObject();
  const char* bandNames[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
  
  for (int i = 0; i < 11; i++) {
    cJSON* band = cJSON_CreateObject();
    cJSON_AddNumberToObject(band, "txCnt", 0);
    cJSON_AddNumberToObject(band, "txMin", 0);
    cJSON_AddItemToObject(bands, bandNames[i], band);
  }
  
  cJSON_AddItemToObject(stats, "bands", bands);
  cJSON_AddItemToObject(status, "stats", stats);
}

static std::string getStringValue(cJSON* obj, const char* key, const char* defaultValue = "") {
  cJSON* item = cJSON_GetObjectItem(obj, key);
  if (item && cJSON_IsString(item)) {
    return std::string(item->valuestring);
  }
  return std::string(defaultValue);
}

static int getIntValue(cJSON* obj, const char* key, int defaultValue = 0) {
  cJSON* item = cJSON_GetObjectItem(obj, key);
  if (item && cJSON_IsNumber(item)) {
    return item->valueint;
  }
  return defaultValue;
}

static void updateStatusFromSettings() {
  // Copy values from settings to status
  cJSON* callItem = cJSON_GetObjectItem(settings, "call");
  if (callItem) {
    cJSON_DeleteItemFromObject(status, "call");
    cJSON_AddItemToObject(status, "call", cJSON_Duplicate(callItem, true));
  }
  
  cJSON* locItem = cJSON_GetObjectItem(settings, "loc");
  if (locItem) {
    cJSON_DeleteItemFromObject(status, "loc");
    cJSON_AddItemToObject(status, "loc", cJSON_Duplicate(locItem, true));
  }
  
  cJSON* pwrItem = cJSON_GetObjectItem(settings, "pwr");
  if (pwrItem) {
    cJSON_DeleteItemFromObject(status, "pwr");
    cJSON_AddItemToObject(status, "pwr", cJSON_Duplicate(pwrItem, true));
  }
  
  cJSON* txPctItem = cJSON_GetObjectItem(settings, "txPct");
  if (txPctItem) {
    cJSON_DeleteItemFromObject(status, "txPct");
    cJSON_AddItemToObject(status, "txPct", cJSON_Duplicate(txPctItem, true));
  }
  
  cJSON* hostItem = cJSON_GetObjectItem(settings, "host");
  if (hostItem) {
    cJSON_DeleteItemFromObject(status, "host");
    cJSON_AddItemToObject(status, "host", cJSON_Duplicate(hostItem, true));
  }
  
  cJSON* wifiModeItem = cJSON_GetObjectItem(settings, "wifiMode");
  if (wifiModeItem) {
    cJSON_DeleteItemFromObject(status, "wifiMode");
    cJSON_AddItemToObject(status, "wifiMode", cJSON_Duplicate(wifiModeItem, true));
  }
  
  // Update WiFi status based on mode
  std::string wifiMode = getStringValue(settings, "wifiMode", "sta");
  if (wifiMode == "ap") {
    cJSON_DeleteItemFromObject(status, "netState");
    cJSON_AddStringToObject(status, "netState", "AP_MODE");
    
    std::string ssidAp = getStringValue(settings, "ssidAp", "WSPR-Beacon");
    cJSON_DeleteItemFromObject(status, "ssid");
    cJSON_AddStringToObject(status, "ssid", ssidAp.c_str());
    
    // Simulate connected clients (varying over time)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    int clientCount = (elapsed / 30) % 4; // 0-3 clients, changes every 30 seconds
    cJSON_DeleteItemFromObject(status, "clientCount");
    cJSON_AddNumberToObject(status, "clientCount", clientCount);
    
    cJSON_DeleteItemFromObject(status, "rssi"); // No RSSI in AP mode
  } else {
    // STA mode - check if we have credentials
    std::string ssid = getStringValue(settings, "ssid", "");
    if (!ssid.empty()) {
      cJSON_DeleteItemFromObject(status, "netState");
      cJSON_AddStringToObject(status, "netState", "READY");
      
      cJSON_DeleteItemFromObject(status, "ssid");
      cJSON_AddStringToObject(status, "ssid", ssid.c_str());
      
      cJSON_DeleteItemFromObject(status, "rssi");
      cJSON_AddNumberToObject(status, "rssi", -65); // Mock RSSI for connected STA
    } else {
      cJSON_DeleteItemFromObject(status, "netState");
      cJSON_AddStringToObject(status, "netState", "AP_MODE"); // Fall back to AP if no STA config
      
      std::string ssidAp = getStringValue(settings, "ssidAp", "WSPR-Beacon");
      cJSON_DeleteItemFromObject(status, "ssid");
      cJSON_AddStringToObject(status, "ssid", ssidAp.c_str());
      
      cJSON_DeleteItemFromObject(status, "clientCount");
      cJSON_AddNumberToObject(status, "clientCount", 0);
      
      cJSON_DeleteItemFromObject(status, "rssi");
    }
    cJSON_DeleteItemFromObject(status, "clientCount"); // No client count in STA mode
  }
}

std::string findWebDirectoryForTestServer() {
  // Get the executable path
  char exePath[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len == -1) {
    std::cerr << "[TestServer] Warning: Could not determine executable path, using current directory" << std::endl;
    return "../../web";
  }
  exePath[len] = '\0';
  
  // Start from the executable directory and search up the tree for web directory
  fs::path currentPath = fs::path(exePath).parent_path();
  
  for (int i = 0; i < 5; ++i) { // Limit search to 5 levels up
    fs::path webPath = currentPath / "web";
    if (fs::exists(webPath) && fs::is_directory(webPath)) {
      std::cout << "[TestServer] Found web directory at: " << webPath << std::endl;
      return webPath.string();
    }
    
    // Go up one level
    fs::path parentPath = currentPath.parent_path();
    if (parentPath == currentPath) {
      // Reached filesystem root
      break;
    }
    currentPath = parentPath;
  }
  
  // Fallback to relative path
  std::cerr << "[TestServer] Warning: Could not find web directory, using fallback ../../web" << std::endl;
  return "../../web";
}

void startTestServer(int port, const std::string& mockDataFile, double timeScale, const std::string& logFile, const std::string& logVerbosity) {
  // Initialize logger with verbosity configuration
  g_logger = std::make_unique<BeaconLogger>(logFile);
  
  if (!logVerbosity.empty()) {
    g_logger->parseVerbosityString(logVerbosity);
  }
  
  // Log initial configuration
  g_logger->logSystemEvent("Logger configuration", g_logger->getConfigurationSummary());
  
  // Initialize time scale
  g_timeScale = timeScale;
  g_serverStartTime = std::chrono::steady_clock::now();
  g_mockStartTime = timeInterface.getTime();
  
  g_logger->logTimeEvent("Server startup", timeScale, g_mockStartTime);
  
  // Initialize default settings first
  initializeDefaultSettings();
  
  // Initialize mock data
  g_logger->logSystemEvent("Loading mock data", "file=" + mockDataFile);
  if (!loadMockData(mockDataFile)) {
    g_logger->logSystemEvent("Mock data load failed, using defaults", "");
    initializeDefaultStatus();
  } else {
    g_logger->logSystemEvent("Mock data loaded successfully", "");
  }
  
  httplib::Server svr;

  svr.Get("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    char* response = cJSON_Print(settings);
    if (response) {
      res.set_content(response, "application/json");
      g_logger->logApiRequest("GET", "/api/settings", 200, std::to_string(strlen(response)) + " bytes");
      free(response);
    } else {
      res.status = 500;
      res.set_content("{\"error\":\"Internal server error\"}", "application/json");
      g_logger->logApiRequest("GET", "/api/settings", 500, "error");
    }
  });

  svr.Post("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    try {
      auto contentType = req.get_header_value("Content-Type");
      g_logger->logVerbose("API", "POST /api/settings received", "content_type=" + contentType + ", body_size=" + std::to_string(req.body.size()));
      
      // Parse as form if the Content-Type is application/x-www-form-urlencoded
      if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        // Form-encoded: use params
        g_logger->logVerbose("SETTINGS", "Processing form-encoded data", "param_count=" + std::to_string(req.params.size()));
        for (auto &item : req.params) {
          cJSON* existingItem = cJSON_GetObjectItem(settings, item.first.c_str());
          std::string oldValue = "null";
          if (existingItem) {
            char* oldStr = cJSON_Print(existingItem);
            if (oldStr) {
              oldValue = std::string(oldStr);
              free(oldStr);
            }
          }
          
          cJSON_DeleteItemFromObject(settings, item.first.c_str());
          cJSON_AddStringToObject(settings, item.first.c_str(), item.second.c_str());
          g_logger->logSettingsChange(item.first, oldValue, item.second);
        }
      } else {
        // Otherwise, treat as JSON
        cJSON* j = cJSON_Parse(req.body.c_str());
        if (!j) {
          throw std::runtime_error("Invalid JSON format");
        }
        
        int fieldCount = cJSON_GetArraySize(j);
        g_logger->logVerbose("SETTINGS", "Processing JSON data", "field_count=" + std::to_string(fieldCount));
        
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, j) {
          if (item->string && !cJSON_IsNull(item)) {
            cJSON* existingItem = cJSON_GetObjectItem(settings, item->string);
            std::string oldValue = "null";
            if (existingItem) {
              char* oldStr = cJSON_Print(existingItem);
              if (oldStr) {
                oldValue = std::string(oldStr);
                free(oldStr);
              }
            }
            
            cJSON_DeleteItemFromObject(settings, item->string);
            cJSON_AddItemToObject(settings, item->string, cJSON_Duplicate(item, true));
            
            char* newStr = cJSON_Print(item);
            std::string newValue = newStr ? std::string(newStr) : "null";
            if (newStr) free(newStr);
            
            g_logger->logSettingsChange(std::string(item->string), oldValue, newValue);
          }
        }
        
        cJSON_Delete(j);
      }
      updateStatusFromSettings();
      g_logger->logBasic("SETTINGS", "Settings update completed successfully", "");
      res.status = 204;
      res.set_content("", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 204);
    } catch (const std::exception& e) {
      g_logger->logBasic("ERROR", "Settings parsing failed", "error=" + std::string(e.what()));
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 400);
    } catch (...) {
      g_logger->logBasic("ERROR", "Unknown settings parsing error", "");
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 400);
    }
  });

  svr.Get("/api/status.json", [](const httplib::Request &req, httplib::Response &res) {
    // Calculate dynamic status based on accelerated time
    cJSON* dynamicStatus = cJSON_Duplicate(status, true);
    
    g_logger->logVerbose("API", "GET /api/status.json processing", "time_scale=" + std::to_string(g_timeScale));
    
    // Calculate elapsed time in mock seconds
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_serverStartTime).count();
    int64_t mockElapsedSeconds = static_cast<int64_t>((elapsed * g_timeScale) / 1000);
    
    // WSPR transmission cycle: 120 seconds (2 minutes)
    // Transmission duration: ~110.6 seconds
    const int WSPR_CYCLE_SECONDS = 120;
    const int WSPR_TX_DURATION = 111;  // Rounded up
    
    // Get TX percentage from settings
    int txPercent = getIntValue(dynamicStatus, "txPct", 20);
    
    // Simple transmission scheduling: transmit every 100/txPercent cycles
    int cycleNumber = mockElapsedSeconds / WSPR_CYCLE_SECONDS;
    int secondsInCycle = mockElapsedSeconds % WSPR_CYCLE_SECONDS;
    
    // Determine if we should transmit this cycle (simple modulo approach)
    bool shouldTransmit = false;
    if (txPercent > 0) {
      int cycleInterval = 100 / txPercent;
      shouldTransmit = (cycleNumber % cycleInterval) == 0;
    }
    
    // Update transmission state
    std::string prevTxState = getStringValue(dynamicStatus, "txState", "UNKNOWN");
    if (shouldTransmit && secondsInCycle < WSPR_TX_DURATION) {
      cJSON_DeleteItemFromObject(dynamicStatus, "txState");
      cJSON_AddStringToObject(dynamicStatus, "txState", "TRANSMITTING");
      cJSON_DeleteItemFromObject(dynamicStatus, "nextTx");
      cJSON_AddNumberToObject(dynamicStatus, "nextTx", 0);
      if (prevTxState != "TRANSMITTING") {
        std::string curBand = getStringValue(dynamicStatus, "curBand", "unknown");
        g_logger->logTransmissionEvent("Transmission started", curBand, 0);
      }
    } else {
      cJSON_DeleteItemFromObject(dynamicStatus, "txState");
      cJSON_AddStringToObject(dynamicStatus, "txState", "IDLE");
      if (prevTxState == "TRANSMITTING") {
        std::string curBand = getStringValue(dynamicStatus, "curBand", "unknown");
        g_logger->logTransmissionEvent("Transmission ended", curBand);
      }
      
      // Calculate next transmission
      if (txPercent > 0) {
        int cycleInterval = 100 / txPercent;
        int nextTxCycle = ((cycleNumber / cycleInterval) + 1) * cycleInterval;
        int secondsUntilNextCycle = (nextTxCycle - cycleNumber) * WSPR_CYCLE_SECONDS - secondsInCycle;
        cJSON_DeleteItemFromObject(dynamicStatus, "nextTx");
        cJSON_AddNumberToObject(dynamicStatus, "nextTx", secondsUntilNextCycle);
        
        // Log next transmission timing if significant change
        static int lastNextTx = -1;
        if (abs(secondsUntilNextCycle - lastNextTx) > 10) {  // Log every 10 second changes
          std::string curBand = getStringValue(dynamicStatus, "curBand", "unknown");
          g_logger->logTransmissionEvent("Next transmission scheduled", curBand, secondsUntilNextCycle);
          lastNextTx = secondsUntilNextCycle;
        }
      } else {
        cJSON_DeleteItemFromObject(dynamicStatus, "nextTx");
        cJSON_AddNumberToObject(dynamicStatus, "nextTx", 9999);  // Never
      }
    }
    
    // Update statistics based on actual transmission time
    // Calculate how many complete transmission cycles have occurred
    int completedTransmissions = 0;
    int totalTransmissionMinutes = 0;
    
    if (txPercent > 0) {
      int cycleInterval = 100 / txPercent;
      completedTransmissions = cycleNumber / cycleInterval;
      
      // Each transmission is ~111 seconds (1.85 minutes)
      totalTransmissionMinutes = completedTransmissions * 2; // Rounded to 2 minutes per TX
      
      // If currently transmitting, add partial time
      if (shouldTransmit && secondsInCycle < WSPR_TX_DURATION) {
        int currentTxMinutes = secondsInCycle / 60;
        totalTransmissionMinutes += currentTxMinutes;
      }
    }
    
    cJSON* stats = cJSON_GetObjectItem(dynamicStatus, "stats");
    if (stats) {
      cJSON_DeleteItemFromObject(stats, "txCnt");
      cJSON_AddNumberToObject(stats, "txCnt", completedTransmissions);
      cJSON_DeleteItemFromObject(stats, "txMin");
      cJSON_AddNumberToObject(stats, "txMin", totalTransmissionMinutes);
    }
    
    // Update WiFi status based on current settings and time
    std::string wifiMode = getStringValue(settings, "wifiMode", "sta");
    if (wifiMode == "ap") {
      cJSON_DeleteItemFromObject(dynamicStatus, "netState");
      cJSON_AddStringToObject(dynamicStatus, "netState", "AP_MODE");
      // Simulate varying client count
      int clientCount = (mockElapsedSeconds / 30) % 4; // 0-3 clients
      cJSON_DeleteItemFromObject(dynamicStatus, "clientCount");
      cJSON_AddNumberToObject(dynamicStatus, "clientCount", clientCount);
      cJSON_DeleteItemFromObject(dynamicStatus, "rssi");
    } else {
      std::string ssid = getStringValue(settings, "ssid", "");
      if (!ssid.empty()) {
        cJSON_DeleteItemFromObject(dynamicStatus, "netState");
        cJSON_AddStringToObject(dynamicStatus, "netState", "READY");
        cJSON_DeleteItemFromObject(dynamicStatus, "ssid");
        cJSON_AddStringToObject(dynamicStatus, "ssid", ssid.c_str());
        // Simulate RSSI variation
        int rssiBase = -65;
        int rssiVariation = (mockElapsedSeconds / 10) % 20 - 10; // ±10 dBm variation
        cJSON_DeleteItemFromObject(dynamicStatus, "rssi");
        cJSON_AddNumberToObject(dynamicStatus, "rssi", rssiBase + rssiVariation);
      } else {
        cJSON_DeleteItemFromObject(dynamicStatus, "netState");
        cJSON_AddStringToObject(dynamicStatus, "netState", "AP_MODE");
        cJSON_DeleteItemFromObject(dynamicStatus, "clientCount");
        cJSON_AddNumberToObject(dynamicStatus, "clientCount", 0);
        cJSON_DeleteItemFromObject(dynamicStatus, "rssi");
      }
      cJSON_DeleteItemFromObject(dynamicStatus, "clientCount");
    }
    
    char* response = cJSON_Print(dynamicStatus);
    if (response) {
      res.set_content(response, "application/json");
      g_logger->logApiRequest("GET", "/api/status.json", 200, std::to_string(strlen(response)) + " bytes");
      free(response);
    } else {
      res.status = 500;
      res.set_content("{\"error\":\"Internal server error\"}", "application/json");
      g_logger->logApiRequest("GET", "/api/status.json", 500, "error");
    }
    cJSON_Delete(dynamicStatus);
  });

  svr.Get("/api/time", [](const httplib::Request &req, httplib::Response &res) {
    int64_t mockTime = getMockTime();
    g_logger->logTimeEvent("Time request processed", g_timeScale, mockTime);
    
    // Simulate NTP sync status
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    
    // Simulate last NTP sync time (every ~10 minutes in mock time)
    int64_t lastSyncInterval = 600; // 10 minutes
    int64_t scaledElapsed = static_cast<int64_t>(elapsed * g_timeScale);
    int64_t timeSinceSync = scaledElapsed % (lastSyncInterval * 2);
    int64_t lastSyncTime = mockTime - timeSinceSync;
    
    cJSON* timeResponse = cJSON_CreateObject();
    cJSON_AddNumberToObject(timeResponse, "unixTime", mockTime);
    std::string isoTime = formatTimeISO(mockTime);
    cJSON_AddStringToObject(timeResponse, "isoTime", isoTime.c_str());
    cJSON_AddBoolToObject(timeResponse, "synced", true);
    std::string lastSyncTimeISO = formatTimeISO(lastSyncTime);
    cJSON_AddStringToObject(timeResponse, "lastSyncTime", lastSyncTimeISO.c_str());
    cJSON_AddNumberToObject(timeResponse, "timeScale", g_timeScale);
    
    char* response = cJSON_Print(timeResponse);
    if (response) {
      res.set_content(response, "application/json");
      g_logger->logApiRequest("GET", "/api/time", 200, std::to_string(strlen(response)) + " bytes");
      free(response);
    } else {
      res.status = 500;
      res.set_content("{\"error\":\"Internal server error\"}", "application/json");
      g_logger->logApiRequest("GET", "/api/time", 500, "error");
    }
    cJSON_Delete(timeResponse);
  });

  svr.Post("/api/wspr/encode", [](const httplib::Request &req, httplib::Response &res) {
    try {
      g_logger->logApiRequest("POST", "/api/wspr/encode", 200, std::to_string(req.body.size()) + " bytes");
      
      cJSON* requestData = cJSON_Parse(req.body.c_str());
      if (!requestData) {
        throw std::runtime_error("Invalid JSON format");
      }
      
      // Extract parameters
      std::string callsign = getStringValue(requestData, "callsign", "N0CALL");
      std::string locator = getStringValue(requestData, "locator", "AA00aa");
      int powerDbm = getIntValue(requestData, "powerDbm", 10);
      uint32_t frequency = static_cast<uint32_t>(getIntValue(requestData, "frequency", 14097100));
      
      g_logger->logBasic("WSPR", "Encoding WSPR message", 
                        "call=" + callsign + ", loc=" + locator + ", pwr=" + std::to_string(powerDbm) + "dBm, freq=" + std::to_string(frequency));
      
      // Create WSPR encoder
      WSPREncoder encoder(frequency);
      
      // Encode the message
      encoder.encode(callsign.c_str(), locator.c_str(), static_cast<int8_t>(powerDbm));
      
      // Build response with symbols
      cJSON* response = cJSON_CreateObject();
      cJSON_AddBoolToObject(response, "success", true);
      cJSON_AddStringToObject(response, "callsign", callsign.c_str());
      cJSON_AddStringToObject(response, "locator", locator.c_str());
      cJSON_AddNumberToObject(response, "powerDbm", powerDbm);
      cJSON_AddNumberToObject(response, "frequency", frequency);
      cJSON_AddNumberToObject(response, "symbolCount", WSPREncoder::TxBufferSize);
      cJSON_AddNumberToObject(response, "toneSpacing", WSPREncoder::ToneSpacing);
      cJSON_AddNumberToObject(response, "symbolPeriod", WSPREncoder::SymbolPeriod);
      
      // Add symbols array
      cJSON* symbolsArray = cJSON_CreateArray();
      for (int i = 0; i < WSPREncoder::TxBufferSize; i++) {
        cJSON_AddItemToArray(symbolsArray, cJSON_CreateNumber(encoder.symbols[i]));
      }
      cJSON_AddItemToObject(response, "symbols", symbolsArray);
      
      // Calculate transmission duration
      uint32_t transmissionDurationMs = WSPREncoder::TxBufferSize * WSPREncoder::SymbolPeriod;
      cJSON_AddNumberToObject(response, "transmissionDurationMs", transmissionDurationMs);
      cJSON_AddNumberToObject(response, "transmissionDurationSeconds", transmissionDurationMs / 1000.0);
      
      g_logger->logVerbose("WSPR", "WSPR encoding completed", 
                          "symbols=" + std::to_string(WSPREncoder::TxBufferSize) + 
                          ", duration=" + std::to_string(transmissionDurationMs) + "ms");
      
      char* responseStr = cJSON_Print(response);
      if (responseStr) {
        res.set_content(responseStr, "application/json");
        g_logger->logApiRequest("POST", "/api/wspr/encode", 200, std::to_string(strlen(responseStr)) + " bytes");
        free(responseStr);
      } else {
        res.status = 500;
        res.set_content("{\"error\":\"Internal server error\"}", "application/json");
        g_logger->logApiRequest("POST", "/api/wspr/encode", 500, "error");
      }
      
      cJSON_Delete(response);
      cJSON_Delete(requestData);
      
    } catch (const std::exception& e) {
      g_logger->logBasic("ERROR", "WSPR encoding failed", "error=" + std::string(e.what()));
      
      cJSON* errorResponse = cJSON_CreateObject();
      cJSON_AddBoolToObject(errorResponse, "success", false);
      cJSON_AddStringToObject(errorResponse, "error", e.what());
      
      char* errorStr = cJSON_Print(errorResponse);
      if (errorStr) {
        res.status = 400;
        res.set_content(errorStr, "application/json");
        free(errorStr);
      } else {
        res.status = 400;
        res.set_content("{\"error\":\"Unknown error\"}", "application/json");
      }
      g_logger->logApiRequest("POST", "/api/wspr/encode", 400, "error");
      cJSON_Delete(errorResponse);
    }
  });

  svr.Get("/api/wifi/scan", [](const httplib::Request &req, httplib::Response &res) {
    // Mock WiFi scan results with time-varying signal strengths
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    
    g_logger->logVerbose("WIFI", "Scan initiated", "elapsed_time=" + std::to_string(elapsed) + "s");
    
    // Simulate signal strength variations over time
    int timeOffset = elapsed / 5; // Changes every 5 seconds
    
    cJSON* scanResults = cJSON_CreateArray();
    
    // Network 1
    cJSON* net1 = cJSON_CreateObject();
    cJSON_AddStringToObject(net1, "ssid", "MyHomeWiFi");
    cJSON_AddNumberToObject(net1, "rssi", -45 + (timeOffset % 10) - 5); // -50 to -40
    cJSON_AddStringToObject(net1, "encryption", "WPA2");
    cJSON_AddNumberToObject(net1, "channel", 6);
    cJSON_AddItemToArray(scanResults, net1);
    
    // Network 2
    cJSON* net2 = cJSON_CreateObject();
    cJSON_AddStringToObject(net2, "ssid", "Neighbor_2.4G");
    cJSON_AddNumberToObject(net2, "rssi", -67 + (timeOffset % 8) - 4); // -71 to -63
    cJSON_AddStringToObject(net2, "encryption", "WPA2");
    cJSON_AddNumberToObject(net2, "channel", 11);
    cJSON_AddItemToArray(scanResults, net2);
    
    // Network 3
    cJSON* net3 = cJSON_CreateObject();
    cJSON_AddStringToObject(net3, "ssid", "CoffeeShop_Guest");
    cJSON_AddNumberToObject(net3, "rssi", -72 + (timeOffset % 6) - 3); // -75 to -69
    cJSON_AddStringToObject(net3, "encryption", "Open");
    cJSON_AddNumberToObject(net3, "channel", 1);
    cJSON_AddItemToArray(scanResults, net3);
    
    // Network 4
    cJSON* net4 = cJSON_CreateObject();
    cJSON_AddStringToObject(net4, "ssid", "TestNetwork_5G");
    cJSON_AddNumberToObject(net4, "rssi", -58 + (timeOffset % 12) - 6); // -64 to -52
    cJSON_AddStringToObject(net4, "encryption", "WPA3");
    cJSON_AddNumberToObject(net4, "channel", 36);
    cJSON_AddItemToArray(scanResults, net4);
    
    // Network 5
    cJSON* net5 = cJSON_CreateObject();
    cJSON_AddStringToObject(net5, "ssid", "Enterprise_Corp");
    cJSON_AddNumberToObject(net5, "rssi", -81 + (timeOffset % 4) - 2); // -83 to -79
    cJSON_AddStringToObject(net5, "encryption", "WPA2-Enterprise");
    cJSON_AddNumberToObject(net5, "channel", 44);
    cJSON_AddItemToArray(scanResults, net5);
    
    // Network 6
    cJSON* net6 = cJSON_CreateObject();
    cJSON_AddStringToObject(net6, "ssid", "WeakSignal_Test");
    cJSON_AddNumberToObject(net6, "rssi", -85 + (timeOffset % 6) - 3); // -88 to -82
    cJSON_AddStringToObject(net6, "encryption", "WPA2");
    cJSON_AddNumberToObject(net6, "channel", 13);
    cJSON_AddItemToArray(scanResults, net6);
    
    // Log detailed scan results
    int networkCount = cJSON_GetArraySize(scanResults);
    std::ostringstream scanDetails;
    scanDetails << "networks=[";
    for (int i = 0; i < networkCount; i++) {
      if (i > 0) scanDetails << ", ";
      cJSON* network = cJSON_GetArrayItem(scanResults, i);
      cJSON* ssid = cJSON_GetObjectItem(network, "ssid");
      cJSON* rssi = cJSON_GetObjectItem(network, "rssi");
      cJSON* encryption = cJSON_GetObjectItem(network, "encryption");
      
      if (ssid && rssi && encryption) {
        scanDetails << ssid->valuestring << "(" << rssi->valueint << "dBm," << encryption->valuestring << ")";
      }
    }
    scanDetails << "]";
    
    g_logger->logWifiScan(networkCount, scanDetails.str());
    
    char* response = cJSON_Print(scanResults);
    if (response) {
      res.set_content(response, "application/json");
      g_logger->logApiRequest("GET", "/api/wifi/scan", 200, std::to_string(strlen(response)) + " bytes");
      free(response);
    } else {
      res.status = 500;
      res.set_content("{\"error\":\"Internal server error\"}", "application/json");
      g_logger->logApiRequest("GET", "/api/wifi/scan", 500, "error");
    }
    cJSON_Delete(scanResults);
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);
  g_logger->logSystemEvent("Web directory configured", "path=" + webDir);

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  
  g_logger->logSystemEvent("HTTP server starting", "port=" + std::to_string(port) + ", address=0.0.0.0");
  
  // Add request logging for all endpoints
  svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
    g_logger->logVerbose("HTTP", "Request received", "method=" + req.method + ", path=" + req.path + ", remote=" + req.get_header_value("Host"));
    return httplib::Server::HandlerResponse::Unhandled;
  });
  
  svr.listen("0.0.0.0", port);
  
  g_logger->logSystemEvent("HTTP server stopped", "");
}
