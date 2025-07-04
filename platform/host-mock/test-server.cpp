#include "test-server.h"
#include "Time.h"
#include <nlohmann/json.hpp>
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

using nlohmann::json;
namespace fs = std::filesystem;

// Comprehensive Logger for beacon operations
class BeaconLogger {
private:
  std::ofstream logFile;
  std::mutex logMutex;
  bool fileLogging;
  
public:
  BeaconLogger(const std::string& logFileName = "") {
    fileLogging = !logFileName.empty();
    if (fileLogging) {
      logFile.open(logFileName, std::ios::out | std::ios::app);
      if (!logFile.is_open()) {
        std::cerr << "Error: Could not open log file: " << logFileName << std::endl;
        fileLogging = false;
      } else {
        log("SYSTEM", "Logger initialized", "file=" + logFileName);
      }
    }
  }
  
  ~BeaconLogger() {
    if (fileLogging && logFile.is_open()) {
      log("SYSTEM", "Logger shutdown", "");
      logFile.close();
    }
  }
  
  void log(const std::string& subsystem, const std::string& event, const std::string& data = "") {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::tm utc_tm;
    #ifdef _WIN32
    gmtime_s(&utc_tm, &time_t_now);
    #else
    gmtime_r(&time_t_now, &utc_tm);
    #endif
    
    std::ostringstream logEntry;
    logEntry << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S");
    logEntry << "." << std::setfill('0') << std::setw(3) << ms.count();
    logEntry << " UTC [" << subsystem << "] " << event;
    if (!data.empty()) {
      logEntry << " | " << data;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (fileLogging && logFile.is_open()) {
      logFile << logEntry.str() << std::endl;
      logFile.flush();  // Ensure immediate write
    }
    
    // Also output to stderr for exceptional events
    if (subsystem == "ERROR" || event.find("Error") != std::string::npos || event.find("Failed") != std::string::npos) {
      std::cerr << logEntry.str() << std::endl;
    }
  }
  
  void logApiRequest(const std::string& method, const std::string& path, int statusCode, const std::string& responseSize = "") {
    std::string data = "method=" + method + ", status=" + std::to_string(statusCode);
    if (!responseSize.empty()) {
      data += ", response_size=" + responseSize;
    }
    log("API", "Request: " + path, data);
  }
  
  void logWifiScan(int networkCount, const std::string& details = "") {
    std::string data = "networks_found=" + std::to_string(networkCount);
    if (!details.empty()) {
      data += ", " + details;
    }
    log("WIFI", "Scan completed", data);
  }
  
  void logTransmissionEvent(const std::string& event, const std::string& band, int64_t nextTxIn = -1) {
    std::string data = "band=" + band;
    if (nextTxIn >= 0) {
      data += ", next_tx_in=" + std::to_string(nextTxIn) + "s";
    }
    log("TX", event, data);
  }
  
  void logTimeEvent(const std::string& event, double timeScale, int64_t mockTime) {
    std::string data = "time_scale=" + std::to_string(timeScale) + ", mock_time=" + std::to_string(mockTime);
    log("TIME", event, data);
  }
  
  void logSettingsChange(const std::string& field, const std::string& oldValue, const std::string& newValue) {
    std::string data = "field=" + field + ", old=" + oldValue + ", new=" + newValue;
    log("SETTINGS", "Configuration changed", data);
  }
};

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

static json settings = {
  {"call", "N0CALL"},
  {"loc", "AA00aa"},
  {"pwr", 23},
  {"txPct", 20},
  {"wifiMode", "sta"},
  {"ssid", ""},
  {"pwd", ""},
  {"ssidAp", "WSPR-Beacon"},
  {"pwdAp", "wspr2024"},
  {"host", "wspr-beacon"},
  {"bands", {
    {"160m", {
      {"en", false},
      {"freq", 1838100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"80m", {
      {"en", false},
      {"freq", 3570100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"40m", {
      {"en", false},
      {"freq", 7040100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"30m", {
      {"en", false},
      {"freq", 10140200},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"20m", {
      {"en", false},
      {"freq", 14097100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"17m", {
      {"en", false},
      {"freq", 18106100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"15m", {
      {"en", false},
      {"freq", 21096100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"12m", {
      {"en", false},
      {"freq", 24926100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"10m", {
      {"en", false},
      {"freq", 28126100},
      {"sched", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }}
  }}
};

static json status;

static bool loadMockData(const std::string& mockDataFile) {
  try {
    g_logger->log("INIT", "Attempting to load mock data", "file=" + mockDataFile);
    std::string mockDataContent = readFile(mockDataFile);
    if (mockDataContent.empty()) {
      g_logger->log("ERROR", "Mock data file not found or empty", "file=" + mockDataFile);
      return false;
    }
    
    json mockData = json::parse(mockDataContent);
    
    // Merge mock data into status, preserving dynamic fields
    status = mockData;
    
    // Also update settings with relevant fields from mock data
    int settingsUpdated = 0;
    std::vector<std::string> updatedFields;
    
    auto updateField = [&](const std::string& field) {
      if (mockData.contains(field)) {
        settings[field] = mockData[field];
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
    
    std::string fieldList = "";
    for (size_t i = 0; i < updatedFields.size(); ++i) {
      if (i > 0) fieldList += ", ";
      fieldList += updatedFields[i];
    }
    g_logger->log("INIT", "Settings updated from mock data", "fields_updated=" + std::to_string(settingsUpdated) + ", fields=[" + fieldList + "]");
    
    // Always set resetTime to current startup time
    status["resetTime"] = formatTimeISO(timeInterface.getStartTime());
    
    g_logger->log("INIT", "Mock data loaded successfully", "file=" + mockDataFile + ", size=" + std::to_string(mockDataContent.size()) + " bytes");
    return true;
  } catch (const std::exception& e) {
    g_logger->log("ERROR", "Mock data parsing failed", "file=" + mockDataFile + ", error=" + std::string(e.what()));
    return false;
  }
}

static void initializeDefaultStatus() {
  // Fallback default status if mock data loading fails
  status = {
    {"call", "N0CALL"},
    {"loc", "AA00aa"},
    {"pwr", 23},
    {"txPct", 20},
    {"host", "wspr-beacon"},
    {"curBand", "20m"},
    {"resetTime", formatTimeISO(timeInterface.getStartTime())},
    {"ssid", "TestWiFi"},
    {"rssi", -70},
    {"netState", "READY"},
    {"stats", {
      {"txCnt", 0},
      {"txMin", 0},
      {"bands", {
        {"160m", {"txCnt", 0, "txMin", 0}},
        {"80m", {"txCnt", 0, "txMin", 0}},
        {"40m", {"txCnt", 0, "txMin", 0}},
        {"30m", {"txCnt", 0, "txMin", 0}},
        {"20m", {"txCnt", 0, "txMin", 0}},
        {"17m", {"txCnt", 0, "txMin", 0}},
        {"15m", {"txCnt", 0, "txMin", 0}},
        {"12m", {"txCnt", 0, "txMin", 0}},
        {"10m", {"txCnt", 0, "txMin", 0}}
      }}
    }}
  };
}

static void updateStatusFromSettings() {
  status["call"] = settings["call"];
  status["loc"] = settings["loc"];
  status["pwr"] = settings["pwr"];
  status["txPct"] = settings["txPct"];
  status["host"] = settings["host"];
  status["wifiMode"] = settings["wifiMode"];
  
  // Update WiFi status based on mode
  std::string wifiMode = settings.value("wifiMode", "sta");
  if (wifiMode == "ap") {
    status["netState"] = "AP_MODE";
    status["ssid"] = settings.value("ssidAp", "WSPR-Beacon");
    // Simulate connected clients (varying over time)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    int clientCount = (elapsed / 30) % 4; // 0-3 clients, changes every 30 seconds
    status["clientCount"] = clientCount;
    status.erase("rssi"); // No RSSI in AP mode
  } else {
    // STA mode - check if we have credentials
    std::string ssid = settings.value("ssid", "");
    if (!ssid.empty()) {
      status["netState"] = "READY";
      status["ssid"] = ssid;
      status["rssi"] = -65; // Mock RSSI for connected STA
    } else {
      status["netState"] = "AP_MODE"; // Fall back to AP if no STA config
      status["ssid"] = settings.value("ssidAp", "WSPR-Beacon");
      status["clientCount"] = 0;
      status.erase("rssi");
    }
    status.erase("clientCount"); // No client count in STA mode
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

void startTestServer(int port, const std::string& mockDataFile, double timeScale, const std::string& logFile) {
  // Initialize logger
  g_logger = std::make_unique<BeaconLogger>(logFile);
  
  // Initialize time scale
  g_timeScale = timeScale;
  g_serverStartTime = std::chrono::steady_clock::now();
  g_mockStartTime = timeInterface.getTime();
  
  g_logger->logTimeEvent("Server startup", timeScale, g_mockStartTime);
  
  // Initialize mock data
  g_logger->log("INIT", "Loading mock data", "file=" + mockDataFile);
  if (!loadMockData(mockDataFile)) {
    g_logger->log("INIT", "Mock data load failed, using defaults", "");
    initializeDefaultStatus();
  } else {
    g_logger->log("INIT", "Mock data loaded successfully", "");
  }
  
  httplib::Server svr;

  svr.Get("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    std::string response = settings.dump();
    res.set_content(response, "application/json");
    g_logger->logApiRequest("GET", "/api/settings", 200, std::to_string(response.size()) + " bytes");
  });

  svr.Post("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    try {
      auto contentType = req.get_header_value("Content-Type");
      g_logger->log("API", "POST /api/settings received", "content_type=" + contentType + ", body_size=" + std::to_string(req.body.size()));
      
      // Parse as form if the Content-Type is application/x-www-form-urlencoded
      if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        // Form-encoded: use params
        g_logger->log("SETTINGS", "Processing form-encoded data", "param_count=" + std::to_string(req.params.size()));
        for (auto &item : req.params) {
          auto oldValue = settings.contains(item.first) ? settings[item.first].dump() : "null";
          settings[item.first] = item.second;
          g_logger->logSettingsChange(item.first, oldValue, item.second);
        }
      } else {
        // Otherwise, treat as JSON
        auto j = json::parse(req.body);
        g_logger->log("SETTINGS", "Processing JSON data", "field_count=" + std::to_string(j.size()));
        for (auto it = j.begin(); it != j.end(); ++it) {
          // Skip null values to avoid overwriting existing settings with nulls
          if (!it.value().is_null()) {
            auto oldValue = settings.contains(it.key()) ? settings[it.key()].dump() : "null";
            settings[it.key()] = it.value();
            g_logger->logSettingsChange(it.key(), oldValue, it.value().dump());
          }
        }
      }
      updateStatusFromSettings();
      g_logger->log("SETTINGS", "Settings update completed successfully", "");
      res.status = 204;
      res.set_content("", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 204);
    } catch (const std::exception& e) {
      g_logger->log("ERROR", "Settings parsing failed", "error=" + std::string(e.what()));
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 400);
    } catch (...) {
      g_logger->log("ERROR", "Unknown settings parsing error", "");
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
      g_logger->logApiRequest("POST", "/api/settings", 400);
    }
  });

  svr.Get("/api/status.json", [](const httplib::Request &req, httplib::Response &res) {
    // Calculate dynamic status based on accelerated time
    json dynamicStatus = status;
    
    g_logger->log("API", "GET /api/status.json processing", "time_scale=" + std::to_string(g_timeScale));
    
    // Calculate elapsed time in mock seconds
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_serverStartTime).count();
    int64_t mockElapsedSeconds = static_cast<int64_t>((elapsed * g_timeScale) / 1000);
    
    // WSPR transmission cycle: 120 seconds (2 minutes)
    // Transmission duration: ~110.6 seconds
    const int WSPR_CYCLE_SECONDS = 120;
    const int WSPR_TX_DURATION = 111;  // Rounded up
    
    // Get TX percentage from settings
    int txPercent = dynamicStatus.value("txPct", 20);
    
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
    std::string prevTxState = dynamicStatus.value("txState", "UNKNOWN");
    if (shouldTransmit && secondsInCycle < WSPR_TX_DURATION) {
      dynamicStatus["txState"] = "TRANSMITTING";
      dynamicStatus["nextTx"] = 0;
      if (prevTxState != "TRANSMITTING") {
        g_logger->logTransmissionEvent("Transmission started", dynamicStatus.value("curBand", "unknown"), 0);
      }
    } else {
      dynamicStatus["txState"] = "IDLE";
      if (prevTxState == "TRANSMITTING") {
        g_logger->logTransmissionEvent("Transmission ended", dynamicStatus.value("curBand", "unknown"));
      }
      
      // Calculate next transmission
      if (txPercent > 0) {
        int cycleInterval = 100 / txPercent;
        int nextTxCycle = ((cycleNumber / cycleInterval) + 1) * cycleInterval;
        int secondsUntilNextCycle = (nextTxCycle - cycleNumber) * WSPR_CYCLE_SECONDS - secondsInCycle;
        dynamicStatus["nextTx"] = secondsUntilNextCycle;
        
        // Log next transmission timing if significant change
        static int lastNextTx = -1;
        if (abs(secondsUntilNextCycle - lastNextTx) > 10) {  // Log every 10 second changes
          g_logger->logTransmissionEvent("Next transmission scheduled", dynamicStatus.value("curBand", "unknown"), secondsUntilNextCycle);
          lastNextTx = secondsUntilNextCycle;
        }
      } else {
        dynamicStatus["nextTx"] = 9999;  // Never
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
    
    dynamicStatus["stats"]["txCnt"] = completedTransmissions;
    dynamicStatus["stats"]["txMin"] = totalTransmissionMinutes;
    
    // Update WiFi status based on current settings and time
    std::string wifiMode = settings.value("wifiMode", "sta");
    if (wifiMode == "ap") {
      dynamicStatus["netState"] = "AP_MODE";
      // Simulate varying client count
      int clientCount = (mockElapsedSeconds / 30) % 4; // 0-3 clients
      dynamicStatus["clientCount"] = clientCount;
      dynamicStatus.erase("rssi");
    } else {
      std::string ssid = settings.value("ssid", "");
      if (!ssid.empty()) {
        dynamicStatus["netState"] = "READY";
        dynamicStatus["ssid"] = ssid;
        // Simulate RSSI variation
        int rssiBase = -65;
        int rssiVariation = (mockElapsedSeconds / 10) % 20 - 10; // ±10 dBm variation
        dynamicStatus["rssi"] = rssiBase + rssiVariation;
      } else {
        dynamicStatus["netState"] = "AP_MODE";
        dynamicStatus["clientCount"] = 0;
        dynamicStatus.erase("rssi");
      }
      dynamicStatus.erase("clientCount");
    }
    
    std::string response = dynamicStatus.dump();
    res.set_content(response, "application/json");
    g_logger->logApiRequest("GET", "/api/status.json", 200, std::to_string(response.size()) + " bytes");
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
    
    json timeResponse = {
      {"unixTime", mockTime},
      {"isoTime", formatTimeISO(mockTime)},
      {"synced", true},
      {"lastSyncTime", formatTimeISO(lastSyncTime)},
      {"timeScale", g_timeScale}
    };
    std::string response = timeResponse.dump();
    res.set_content(response, "application/json");
    g_logger->logApiRequest("GET", "/api/time", 200, std::to_string(response.size()) + " bytes");
  });

  svr.Get("/api/wifi/scan", [](const httplib::Request &req, httplib::Response &res) {
    // Mock WiFi scan results with time-varying signal strengths
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    
    g_logger->log("WIFI", "Scan initiated", "elapsed_time=" + std::to_string(elapsed) + "s");
    
    // Simulate signal strength variations over time
    int timeOffset = elapsed / 5; // Changes every 5 seconds
    
    json scanResults = json::array({
      {
        {"ssid", "MyHomeWiFi"},
        {"rssi", -45 + (timeOffset % 10) - 5}, // -50 to -40
        {"encryption", "WPA2"},
        {"channel", 6}
      },
      {
        {"ssid", "Neighbor_2.4G"},
        {"rssi", -67 + (timeOffset % 8) - 4}, // -71 to -63
        {"encryption", "WPA2"},
        {"channel", 11}
      },
      {
        {"ssid", "CoffeeShop_Guest"},
        {"rssi", -72 + (timeOffset % 6) - 3}, // -75 to -69
        {"encryption", "Open"},
        {"channel", 1}
      },
      {
        {"ssid", "TestNetwork_5G"},
        {"rssi", -58 + (timeOffset % 12) - 6}, // -64 to -52
        {"encryption", "WPA3"},
        {"channel", 36}
      },
      {
        {"ssid", "Enterprise_Corp"},
        {"rssi", -81 + (timeOffset % 4) - 2}, // -83 to -79
        {"encryption", "WPA2-Enterprise"},
        {"channel", 44}
      },
      {
        {"ssid", "WeakSignal_Test"},
        {"rssi", -85 + (timeOffset % 6) - 3}, // -88 to -82
        {"encryption", "WPA2"},
        {"channel", 13}
      }
    });
    
    // Log detailed scan results
    std::ostringstream scanDetails;
    scanDetails << "networks=[";
    for (size_t i = 0; i < scanResults.size(); ++i) {
      if (i > 0) scanDetails << ", ";
      auto network = scanResults[i];
      scanDetails << network["ssid"].get<std::string>()
                  << "(" << network["rssi"].get<int>() << "dBm,"
                  << network["encryption"].get<std::string>() << ")";
    }
    scanDetails << "]";
    
    g_logger->logWifiScan(scanResults.size(), scanDetails.str());
    
    std::string response = scanResults.dump();
    res.set_content(response, "application/json");
    g_logger->logApiRequest("GET", "/api/wifi/scan", 200, std::to_string(response.size()) + " bytes");
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);
  g_logger->log("SERVER", "Web directory configured", "path=" + webDir);

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  
  g_logger->log("SERVER", "HTTP server starting", "port=" + std::to_string(port) + ", address=0.0.0.0");
  
  // Add request logging for all endpoints
  svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
    g_logger->log("HTTP", "Request received", "method=" + req.method + ", path=" + req.path + ", remote=" + req.get_header_value("Host"));
    return httplib::Server::HandlerResponse::Unhandled;
  });
  
  svr.listen("0.0.0.0", port);
  
  g_logger->log("SERVER", "HTTP server stopped", "");
}
