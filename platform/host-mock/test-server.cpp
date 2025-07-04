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
  {"callsign", "N0CALL"},
  {"locator", "AA00aa"},
  {"powerDbm", 23},
  {"txPercent", 20},
  {"wifiSsid", ""},
  {"wifiPassword", ""},
  {"hostname", "wspr-beacon"},
  {"bands", {
    {"160m", {
      {"enabled", false},
      {"frequency", 1838100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"80m", {
      {"enabled", false},
      {"frequency", 3570100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"40m", {
      {"enabled", false},
      {"frequency", 7040100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"30m", {
      {"enabled", false},
      {"frequency", 10140200},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"20m", {
      {"enabled", false},
      {"frequency", 14097100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"17m", {
      {"enabled", false},
      {"frequency", 18106100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"15m", {
      {"enabled", false},
      {"frequency", 21096100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"12m", {
      {"enabled", false},
      {"frequency", 24926100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
    }},
    {"10m", {
      {"enabled", false},
      {"frequency", 28126100},
      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}
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
    
    // Always set lastResetTime to current startup time
    status["lastResetTime"] = formatTimeISO(timeInterface.getStartTime());
    
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
    {"callsign", "N0CALL"},
    {"locator", "AA00aa"},
    {"powerDbm", 23},
    {"txPercent", 20},
    {"hostname", "wspr-beacon"},
    {"currentBand", "20m"},
    {"lastResetTime", formatTimeISO(timeInterface.getStartTime())},
    {"wifiSsid", "TestWiFi"},
    {"wifiRssi", -70},
    {"networkState", "READY"},
    {"statistics", {
      {"totalTransmissions", 0},
      {"totalMinutes", 0},
      {"byBand", {
        {"160m", {"transmissions", 0, "minutes", 0}},
        {"80m", {"transmissions", 0, "minutes", 0}},
        {"40m", {"transmissions", 0, "minutes", 0}},
        {"30m", {"transmissions", 0, "minutes", 0}},
        {"20m", {"transmissions", 0, "minutes", 0}},
        {"17m", {"transmissions", 0, "minutes", 0}},
        {"15m", {"transmissions", 0, "minutes", 0}},
        {"12m", {"transmissions", 0, "minutes", 0}},
        {"10m", {"transmissions", 0, "minutes", 0}}
      }}
    }}
  };
}

static void updateStatusFromSettings() {
  status["callsign"] = settings["callsign"];
  status["locator"] = settings["locator"];
  status["powerDbm"] = settings["powerDbm"];
  status["txPercent"] = settings["txPercent"];
  status["hostname"] = settings["hostname"];
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
    int txPercent = dynamicStatus.value("txPercent", 20);
    
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
      dynamicStatus["transmissionState"] = "TRANSMITTING";
      dynamicStatus["nextTransmissionIn"] = 0;
    } else {
      dynamicStatus["transmissionState"] = "IDLE";
      
      // Calculate next transmission
      if (txPercent > 0) {
        int cycleInterval = 100 / txPercent;
        int nextTxCycle = ((cycleNumber / cycleInterval) + 1) * cycleInterval;
        int secondsUntilNextCycle = (nextTxCycle - cycleNumber) * WSPR_CYCLE_SECONDS - secondsInCycle;
        dynamicStatus["nextTransmissionIn"] = secondsUntilNextCycle;
      } else {
        dynamicStatus["nextTransmissionIn"] = 9999;  // Never
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
    
    dynamicStatus["statistics"]["totalTransmissions"] = completedTransmissions;
    dynamicStatus["statistics"]["totalMinutes"] = totalTransmissionMinutes;
    
    res.set_content(dynamicStatus.dump(), "application/json");
  });

  svr.Get("/api/time", [](const httplib::Request &, httplib::Response &res) {
    int64_t mockTime = getMockTime();
    json timeResponse = {
      {"unixTime", mockTime},
      {"isoTime", formatTimeISO(mockTime)},
      {"synced", true}
    };
    res.set_content(timeResponse.dump(), "application/json");
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  svr.listen("0.0.0.0", port);
}
