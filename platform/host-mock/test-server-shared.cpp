#include "test-server.h"
#include "Time.h"
#include "Settings.h"
#include "HttpHandlerIntf.h"
#include "HostMockHttpWrappers.h"
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
#include <memory>
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
static std::unique_ptr<Time> g_timeInterface;
static std::unique_ptr<Settings> g_settingsInterface;
static std::unique_ptr<HostMockHttpEndpointHandler> g_endpointHandler;
static double g_timeScale = 1.0;
static std::unique_ptr<BeaconLogger> g_logger;

static std::string readFile(const std::string &path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.good()) return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

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
    
    // Load settings from mock data
    int settingsUpdated = 0;
    std::vector<std::string> updatedFields;
    
    auto updateField = [&](const std::string& field) {
      cJSON* item = cJSON_GetObjectItem(mockData, field.c_str());
      if (item) {
        if (cJSON_IsString(item)) {
          g_settingsInterface->setString(field.c_str(), item->valuestring);
        } else if (cJSON_IsNumber(item)) {
          g_settingsInterface->setInt(field.c_str(), item->valueint);
        } else if (cJSON_IsBool(item)) {
          g_settingsInterface->setBool(field.c_str(), cJSON_IsTrue(item));
        }
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
    
    g_logger->logSystemEvent("Mock data loaded successfully", "file=" + mockDataFile + ", size=" + std::to_string(mockDataContent.size()) + " bytes");
    
    cJSON_Delete(mockData);
    return true;
  } catch (const std::exception& e) {
    g_logger->logBasic("ERROR", "Mock data parsing failed", "file=" + mockDataFile + ", error=" + std::string(e.what()));
    return false;
  }
}

static void initializeDefaultSettings() {
  g_settingsInterface->setString("call", "N0CALL");
  g_settingsInterface->setString("loc", "AA00aa");
  g_settingsInterface->setInt("pwr", 23);
  g_settingsInterface->setInt("txPct", 20);
  g_settingsInterface->setString("bandMode", "sequential");
  g_settingsInterface->setString("wifiMode", "sta");
  g_settingsInterface->setString("ssid", "");
  g_settingsInterface->setString("pwd", "");
  g_settingsInterface->setString("ssidAp", "WSPR-Beacon");
  g_settingsInterface->setString("pwdAp", "wspr2024");
  g_settingsInterface->setString("host", "wspr-beacon");
  
  // Initialize band settings with defaults
  const char* bandNames[] = {"160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
  const int bandFreqs[] = {1836600, 3568600, 5287200, 7038600, 10138700, 14095600, 18104600, 21094600, 24924600, 28124600, 50293000, 144488500};
  
  for (int i = 0; i < (int)(sizeof(bandNames) / sizeof(bandNames[0])); i++) {
    std::string bandPrefix = std::string("band") + bandNames[i];
    g_settingsInterface->setBool((bandPrefix + "En").c_str(), false);
    g_settingsInterface->setInt((bandPrefix + "Freq").c_str(), bandFreqs[i]);
    g_settingsInterface->setInt((bandPrefix + "Sched").c_str(), 16777215);
  }
  
  g_settingsInterface->store();
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

void startTestServerShared(int port, const std::string& mockDataFile, double timeScale, const std::string& logFile, const std::string& logVerbosity) {
  // Initialize logger with verbosity configuration
  g_logger = std::make_unique<BeaconLogger>(logFile);
  
  if (!logVerbosity.empty()) {
    g_logger->parseVerbosityString(logVerbosity);
  }
  
  // Log initial configuration
  g_logger->logSystemEvent("Logger configuration", g_logger->getConfigurationSummary());
  
  // Initialize time interface
  g_timeInterface = std::make_unique<Time>();
  
  // Initialize settings interface
  g_settingsInterface = std::make_unique<Settings>();
  
  // Initialize endpoint handler with time scaling
  g_endpointHandler = std::make_unique<HostMockHttpEndpointHandler>(
    g_settingsInterface.get(), g_timeInterface.get(), timeScale
  );
  
  g_logger->logTimeEvent("Server startup", timeScale, g_timeInterface->getTime());
  
  // Initialize default settings first
  initializeDefaultSettings();
  
  // Load mock data if specified
  g_logger->logSystemEvent("Loading mock data", "file=" + mockDataFile);
  if (!loadMockData(mockDataFile)) {
    g_logger->logSystemEvent("Mock data load failed, using defaults", "");
  } else {
    g_logger->logSystemEvent("Mock data loaded successfully", "");
  }
  
  httplib::Server svr;

  // Use shared endpoint handlers
  svr.Get("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiSettings(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("GET", "/api/settings", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("GET", "/api/settings", res.status, "error");
    }
  });

  svr.Post("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiSettings(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("POST", "/api/settings", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("POST", "/api/settings", res.status, "error");
    }
  });

  svr.Get("/api/status.json", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiStatus(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("GET", "/api/status.json", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("GET", "/api/status.json", res.status, "error");
    }
  });

  svr.Get("/api/time", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiTime(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("GET", "/api/time", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("GET", "/api/time", res.status, "error");
    }
  });

  svr.Get("/api/wifi/scan", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiWifiScan(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("GET", "/api/wifi/scan", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("GET", "/api/wifi/scan", res.status, "error");
    }
  });

  svr.Post("/api/wspr/encode", [](const httplib::Request &req, httplib::Response &res) {
    HostMockHttpRequest request(req);
    HostMockHttpResponse response(res);
    
    HttpHandlerResult result = g_endpointHandler->handleApiWSPREncode(&request, &response);
    if (result == HttpHandlerResult::OK) {
      g_logger->logApiRequest("POST", "/api/wspr/encode", res.status, "shared handler");
    } else {
      g_logger->logApiRequest("POST", "/api/wspr/encode", res.status, "error");
    }
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);
  g_logger->logSystemEvent("Web directory configured", "path=" + webDir);

  std::cout << "Host testbench web server (shared endpoints) running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  
  g_logger->logSystemEvent("HTTP server starting", "port=" + std::to_string(port) + ", address=0.0.0.0, implementation=shared");
  
  // Add request logging for all endpoints
  svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
    g_logger->logVerbose("HTTP", "Request received", "method=" + req.method + ", path=" + req.path + ", remote=" + req.get_header_value("Host"));
    return httplib::Server::HandlerResponse::Unhandled;
  });
  
  svr.listen("0.0.0.0", port);
  
  g_logger->logSystemEvent("HTTP server stopped", "");
}