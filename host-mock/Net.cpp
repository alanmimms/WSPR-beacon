#include "Net.h"
#include <cstdio>
#include <cstring>

Net::Net() : connected(false), serverStarted(false) {
  for (int i = 0; i < maxClients; i++) {
    clientConnected[i] = false;
  }
}

Net::~Net() {}

bool Net::init() {
  printf("[NetHostMock] init called\n");
  return true;
}

bool Net::connect(const char *ssid, const char *password) {
  printf("[NetHostMock] connect to SSID '%s' with password '%s'\n", ssid, password);
  connected = true;
  return true;
}

bool Net::disconnect() {
  printf("[NetHostMock] disconnect called\n");
  connected = false;
  return true;
}

bool Net::isConnected() {
  printf("[NetHostMock] isConnected called, returning %d\n", connected ? 1 : 0);
  return connected;
}

bool Net::startServer(int port) {
  printf("[NetHostMock] startServer on port %d\n", port);
  serverStarted = true;
  return true;
}

void Net::stopServer() {
  printf("[NetHostMock] stopServer called\n");
  serverStarted = false;
  for (int i = 0; i < maxClients; i++) {
    clientConnected[i] = false;
  }
}

int Net::send(int clientId, const void *data, int len) {
  if (clientId < 0 || clientId >= maxClients || !clientConnected[clientId]) {
    printf("[NetHostMock] send: invalid clientId %d\n", clientId);
    return -1;
  }
  printf("[NetHostMock] send to client %d, %d bytes\n", clientId, len);
  return len;
}

int Net::receive(int clientId, void *buffer, int maxLen) {
  if (clientId < 0 || clientId >= maxClients || !clientConnected[clientId]) {
    printf("[NetHostMock] receive: invalid clientId %d\n", clientId);
    return -1;
  }
  // Mock: fill buffer with zeros, return maxLen
  memset(buffer, 0, maxLen);
  printf("[NetHostMock] receive from client %d, %d bytes\n", clientId, maxLen);
  return maxLen;
}

void Net::closeClient(int clientId) {
  if (clientId < 0 || clientId >= maxClients) {
    printf("[NetHostMock] closeClient: invalid clientId %d\n", clientId);
    return;
  }
  clientConnected[clientId] = false;
  printf("[NetHostMock] closeClient %d\n", clientId);
}

int Net::waitForClient() {
  for (int i = 0; i < maxClients; i++) {
    if (!clientConnected[i]) {
      clientConnected[i] = true;
      printf("[NetHostMock] waitForClient: accepted client %d\n", i);
      return i;
    }
  }
  printf("[NetHostMock] waitForClient: no free client slots\n");
  return -1;
}
