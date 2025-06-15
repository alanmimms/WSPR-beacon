#pragma once

#include <cstdint>

class Settings {
public:
  Settings();

  void load();
  void save();

  const char* getCallsign() const;
  void setCallsign(const char* callsign);

  const char* getLocator() const;
  void setLocator(const char* locator);

  const char* getSsid() const;
  void setSsid(const char* ssid);

  const char* getPassword() const;
  void setPassword(const char* password);

private:
  static const int MAX_CALLSIGN_LEN = 12;
  static const int MAX_LOCATOR_LEN = 7;
  static const int MAX_SSID_LEN = 32;
  static const int MAX_PASSWORD_LEN = 64;

  char callsign[MAX_CALLSIGN_LEN];
  char locator[MAX_LOCATOR_LEN];
  char ssid[MAX_SSID_LEN];
  char password[MAX_PASSWORD_LEN];
};