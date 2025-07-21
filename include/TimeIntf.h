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

  // Returns the last successful sync time (0 if never synced)
  virtual int64_t getLastSyncTime() = 0;
  
  // Time utility methods to avoid direct use of C time functions
  
  // Convert UNIX timestamp to UTC struct tm, returns true on success
  virtual bool getUTCTime(int64_t unixTime, struct tm* result) = 0;
  
  // Get current UTC hour (0-23)
  virtual int getCurrentUTCHour() = 0;
  
  // Get UTC hour for a specific timestamp
  virtual int getUTCHour(int64_t unixTime) = 0;
  
  // Format time as ISO 8601 string (YYYY-MM-DDTHH:MM:SSZ)
  virtual const char* formatTimeISO(int64_t unixTime) = 0;
};