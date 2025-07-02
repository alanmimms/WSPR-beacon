#include "test-server.h"
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <atomic>
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
  {"powerDbm", 23}
};

static json status = {
  {"callsign", "N0CALL"},
  {"locator", "AA00aa"},
  {"powerDbm", 23},
  {"hostname", "testbench.local"}
};

static void updateStatusFromSettings() {
  status["callsign"] = settings["callsign"];
  status["locator"] = settings["locator"];
  status["powerDbm"] = settings["powerDbm"];
}

void startTestServer(int port) {
  httplib::Server svr;

  svr.Get("/api/settings", [](const httplib::Request &, httplib::Response &res) {
    res.set_content(settings.dump(), "application/json");
  });

  svr.Post("/api/settings", [](const httplib::Request &req, httplib::Response &res) {
    try {
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
          settings[it.key()] = it.value();
        }
      }
      updateStatusFromSettings();
      res.status = 204;
      res.set_content("", "application/json");
    } catch (...) {
      res.status = 400;
      res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
    }
  });

  svr.Get("/api/status.json", [](const httplib::Request &, httplib::Response &res) {
    res.set_content(status.dump(), "application/json");
  });

  // Serve static files - assumes cwd is project root, looks in ../../web
  svr.set_mount_point("/", "../../web");

  std::cout << "Host testbench web server running at http://localhost:" << port << std::endl;
  std::cout << "Press Ctrl+C to stop." << std::endl;
  svr.listen("0.0.0.0", port);
}
