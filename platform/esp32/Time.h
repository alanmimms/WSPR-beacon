#pragma once

#include "TimeIntf.h"
#include <atomic>
#include <chrono>

/**
 * ESP32 implementation of TimeIntf using ESP-IDF SNTP and time APIs.
 * Provides SNTP synchronization and tracks when the application started.
 */
class Time : public TimeIntf {
public:
  Time();
  ~Time() override = default;

  // Returns current UNIX time (seconds since epoch)
  int64_t getTime() override;

  // Sets system time, returns true on success
  bool setTime(int64_t unixTime) override;

  // Fills struct tm for UTC time. Returns true on success.
  bool getLocalTime(struct tm *result) override;

  // Triggers SNTP sync (async), returns true if started okay
  bool syncTime(const char *ntpServer) override;

  // Returns true if time is considered synchronized by SNTP
  bool isTimeSynced() override;

  // Returns the last successful sync time (0 if never synced)
  int64_t getLastSyncTime() override;

  // New time utility methods
  bool getUTCTime(int64_t unixTime, struct tm* result) override;
  int getCurrentUTCHour() override;
  int getUTCHour(int64_t unixTime) override;
  const char* formatTimeISO(int64_t unixTime) override;

  // Returns the time when this Time object was created (boot time)
  int64_t getStartTime();

private:
  static void sntpTimeSyncNotificationCallback(struct timeval *tv);
  static Time* instance;
  
  std::chrono::steady_clock::time_point bootTime;
  std::atomic<bool> timeSynced{false};
  std::atomic<int64_t> firstSyncTime{0};
  std::atomic<int64_t> lastSyncTime{0};
  
  void initializeSNTP();
};