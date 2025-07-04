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
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using nlohmann::json;
namespace fs = std::filesystem;

static std::atomic<bool> running{true};
static Time timeInterface;
static double g_timeScale = 1.0;
static std::chrono::steady_clock::time_point g_serverStartTime;
static int64_t g_mockStartTime;

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
    std::string mockDataContent = readFile(mockDataFile);
    if (mockDataContent.empty()) {
      std::cerr << "[TestServer] Warning: Mock data file '" << mockDataFile << "' not found or empty, using defaults" << std::endl;
      return false;
    }
    
    json mockData = json::parse(mockDataContent);
    
    // Merge mock data into status, preserving dynamic fields
    status = mockData;
    
    // Also update settings with relevant fields from mock data
    if (mockData.contains("call")) settings["call"] = mockData["call"];
    if (mockData.contains("loc")) settings["loc"] = mockData["loc"];
    if (mockData.contains("pwr")) settings["pwr"] = mockData["pwr"];
    if (mockData.contains("txPct")) settings["txPct"] = mockData["txPct"];
    if (mockData.contains("host")) settings["host"] = mockData["host"];
    if (mockData.contains("wifiMode")) settings["wifiMode"] = mockData["wifiMode"];
    if (mockData.contains("ssid")) settings["ssid"] = mockData["ssid"];
    if (mockData.contains("ssidAp")) settings["ssidAp"] = mockData["ssidAp"];
    if (mockData.contains("pwdAp")) settings["pwdAp"] = mockData["pwdAp"];
    
    // Always set resetTime to current startup time
    status["resetTime"] = formatTimeISO(timeInterface.getStartTime());
    
    std::cout << "[TestServer] Loaded mock data from: " << mockDataFile << std::endl;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "[TestServer] Error loading mock data: " << e.what() << std::endl;
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

void startTestServer(int port, const std::string& mockDataFile, double timeScale) {
  // Initialize time scale
  g_timeScale = timeScale;
  g_serverStartTime = std::chrono::steady_clock::now();
  g_mockStartTime = timeInterface.getTime();
  
  // Initialize mock data
  if (!loadMockData(mockDataFile)) {
    initializeDefaultStatus();
  }
  
  httplib::Server svr;

  svr.Get("/api/settings", [](const httplib::Request &, httplib::Response &res) {
    res.set_content(settings.dump(), "application/json");
  });

  svr.Post("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    try {
      std::cout << "[TestServer] POST /api/settings - Content-Type: " << req.get_header_value("Content-Type") << std::endl;
      std::cout << "[TestServer] Body: " << req.body << std::endl;
      
      // Parse as form if the Content-Type is application/x-www-form-urlencoded
      auto contentType = req.get_header_value("Content-Type");
      if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        // Form-encoded: use params
        for (auto &item : req.params) {
          settings[item.first] = item.second;
        }
      } else {
        // Otherwise, treat as JSON
        auto j = json::parse(req.body);
        for (auto it = j.begin(); it != j.end(); ++it) {
          // Skip null values to avoid overwriting existing settings with nulls
          if (!it.value().is_null()) {
            settings[it.key()] = it.value();
          }
        }
      }
      updateStatusFromSettings();
      std::cout << "[TestServer] Settings updated successfully" << std::endl;
      res.status = 204;
      res.set_content("", "application/json");
    } catch (const std::exception& e) {
      std::cout << "[TestServer] Error parsing settings: " << e.what() << std::endl;
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
    } catch (...) {
      std::cout << "[TestServer] Unknown error parsing settings" << std::endl;
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
    }
  });

  svr.Get("/api/status.json", [](const httplib::Request &, httplib::Response &res) {
    // Calculate dynamic status based on accelerated time
    json dynamicStatus = status;
    
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
    if (shouldTransmit && secondsInCycle < WSPR_TX_DURATION) {
      dynamicStatus["txState"] = "TRANSMITTING";
      dynamicStatus["nextTx"] = 0;
    } else {
      dynamicStatus["txState"] = "IDLE";
      
      // Calculate next transmission
      if (txPercent > 0) {
        int cycleInterval = 100 / txPercent;
        int nextTxCycle = ((cycleNumber / cycleInterval) + 1) * cycleInterval;
        int secondsUntilNextCycle = (nextTxCycle - cycleNumber) * WSPR_CYCLE_SECONDS - secondsInCycle;
        dynamicStatus["nextTx"] = secondsUntilNextCycle;
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
    
    res.set_content(dynamicStatus.dump(), "application/json");
  });

  svr.Get("/api/time", [](const httplib::Request &, httplib::Response &res) {
    int64_t mockTime = getMockTime();
    
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
    res.set_content(timeResponse.dump(), "application/json");
  });

  svr.Get("/api/wifi/scan", [](const httplib::Request &, httplib::Response &res) {
    // Mock WiFi scan results with time-varying signal strengths
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_serverStartTime).count();
    
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
    
    std::cout << "[TestServer] WiFi scan requested - returning " << scanResults.size() << " networks" << std::endl;
    res.set_content(scanResults.dump(), "application/json");
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  svr.listen("0.0.0.0", port);
}
