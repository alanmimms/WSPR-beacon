#pragma once

#include <stdint.h>

/**
 * Abstract interface for SNTP and system time.
 * Allows application code to get/set/query time and request SNTP sync.
 * Host implementation: uses standard C time APIs and NTP.
 * Target implementation: wraps ESP-IDF SNTP and time APIs.
 */
class TimeIntf {
public:
  virtual ~TimeIntf() {}

  // Returns current UNIX time (seconds since epoch)
  virtual int64_t getTime() = 0;

  // Sets system time, returns true on success
  virtual bool setTime(int64_t unixTime) = 0;

  // Fills struct tm for local time. Returns true on success.
  virtual bool getLocalTime(struct tm *result) = 0;

  // Triggers SNTP sync (may be async), returns true if started okay
  virtual bool syncTime(const char *ntpServer) = 0;

  // Returns true if time is considered synchronized by SNTP/NTP
  virtual bool isTimeSynced() = 0;
};