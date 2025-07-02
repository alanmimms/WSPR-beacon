#include "test-server.h"
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <limits.h>
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

static std::string readFile(const std::string &path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.good()) return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static json settings = {
  {"callsign", "N0CALL"},
  {"locator", "AA00aa"},
  {"powerDbm", 23},
  {"txPercent", 20},
  {"wifiSsid", ""},
  {"wifiPassword", ""},
  {"hostname", "wspr-beacon"}
};

static json status = {
  {"callsign", "N0CALL"},
  {"locator", "AA00aa"},
  {"powerDbm", 23},
  {"txPercent", 20},
  {"hostname", "wspr-beacon"}
};

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

void startTestServer(int port) {
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
    res.set_content(status.dump(), "application/json");
  });

  // Serve static files - dynamically find web directory
  std::string webDir = findWebDirectoryForTestServer();
  svr.set_mount_point("/", webDir);

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  svr.listen("0.0.0.0", port);
}
