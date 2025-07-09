#include "WebServer.h"
#include <httplib.h>
#include "cJSON.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <limits.h>

namespace fs = std::filesystem;

std::string findWebDirectory() {
  // Get the executable path
  char exePath[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len == -1) {
    std::cerr << "[WebServer] Warning: Could not determine executable path, using current directory" << std::endl;
    return "../web";
  }
  exePath[len] = '\0';
  
  // Start from the executable directory and search up the tree for web directory
  fs::path currentPath = fs::path(exePath).parent_path();
  
  for (int i = 0; i < 5; ++i) { // Limit search to 5 levels up
    fs::path webPath = currentPath / "web";
    if (fs::exists(webPath) && fs::is_directory(webPath)) {
      std::cout << "[WebServer] Found web directory at: " << webPath << std::endl;
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
  std::cerr << "[WebServer] Warning: Could not find web directory, using fallback ../web" << std::endl;
  return "../web";
}

WebServer::WebServer(SettingsIntf *settings)
  : settings(settings), scheduler(nullptr), beacon(nullptr), running(false) {}

WebServer::~WebServer() {
  stop();
}

void WebServer::setSettingsChangedCallback(const std::function<void()> &cb) {
  settingsChangedCallback = cb;
}

void WebServer::setScheduler(Scheduler* sched) {
  scheduler = sched;
}

void WebServer::setBeacon(Beacon* beaconInstance) {
  beacon = beaconInstance;
}

void WebServer::updateBeaconState(const char* networkState, const char* transmissionState, const char* band, uint32_t frequency) {
  // Stub implementation for host-mock - could log or store state if needed
  std::cout << "[WebServer] Beacon state update: " << networkState << " / " << transmissionState 
            << " on " << band << " (" << (frequency / 1000000.0) << " MHz)" << std::endl;
}

void WebServer::start() {
  if (running) return;
  running = true;
  serverThread = std::thread([this]() {
    httplib::Server svr;

    svr.Get("/api/settings", [this](const httplib::Request &, httplib::Response &res) {
      char *jsonStr = settings->toJsonString();
      res.set_content(jsonStr, "application/json");
      free(jsonStr);
    });

    svr.Post("/api/settings", [this](const httplib::Request &req, httplib::Response &res) {
      if (settings->fromJsonString(req.body.c_str())) {
        settings->store();
        if (settingsChangedCallback) settingsChangedCallback();
        res.status = 204;
        res.set_content("", "application/json");
      } else {
        res.status = 400;
        res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
      }
    });

    svr.Get("/api/status.json", [this](const httplib::Request &, httplib::Response &res) {
      char *jsonStr = settings->toJsonString();
      res.set_content(jsonStr, "application/json");
      free(jsonStr);
    });

    std::string webDir = findWebDirectory();
    svr.set_mount_point("/", webDir);

    std::cout << "[WebServerMock] Listening on http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    running = false;
  });
}

void WebServer::stop() {
  if (running) {
    running = false;
    // No direct stop in cpp-httplib, so thread will exit when server is killed externally
    if (serverThread.joinable()) {
      serverThread.detach();
    }
  }
}