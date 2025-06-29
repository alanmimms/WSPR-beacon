#include "WebServer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <atomic>
#include <thread>

using nlohmann::json;

WebServer::WebServer(SettingsIntf *settings)
  : settings(settings), running(false) {}

WebServer::~WebServer() {
  stop();
}

void WebServer::setSettingsChangedCallback(const std::function<void()> &cb) {
  settingsChangedCallback = cb;
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

    svr.set_mount_point("/", "../web");

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