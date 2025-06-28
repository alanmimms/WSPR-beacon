#pragma once

class NetIntf {
public:
  virtual ~NetIntf() {}

  virtual bool init() = 0;
  virtual bool connect(const char *ssid, const char *password) = 0;
  virtual bool disconnect() = 0;
  virtual bool isConnected() = 0;
  virtual bool startServer(int port) = 0;
  virtual void stopServer() = 0;
  virtual int send(int clientId, const void *data, int len) = 0;
  virtual int receive(int clientId, void *buffer, int maxLen) = 0;
  virtual void closeClient(int clientId) = 0;
  virtual int waitForClient() = 0;
};