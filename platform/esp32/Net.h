#pragma once

#include "NetIntf.h"
#include <esp_wifi.h>
#include <esp_netif.h>

class Net : public NetIntf {
public:
  Net();
  ~Net() override;

  bool init() override;
  bool connect(const char *ssid, const char *password) override;
  bool disconnect() override;
  bool isConnected() override;
  bool startServer(int port) override;
  void stopServer() override;
  int send(int clientId, const void *data, int len) override;
  int receive(int clientId, void *buffer, int maxLen) override;
  void closeClient(int clientId) override;
  int waitForClient() override;

private:
  // WiFi state, server sockets, etc
};