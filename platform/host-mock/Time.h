#pragma once

#include "TimeIntf.h"
#include <chrono>
#include <atomic>

/**
 * Host mock implementation of TimeIntf using standard C++ time libraries.
 * Uses system time (UTC) and tracks when the application started for reset time.
 */
class Time : public TimeIntf {
public:
  Time();
  ~Time() override = default;

  // Returns current UNIX time (seconds since epoch)
  int64_t getTime() override;

  // Sets system time (not supported on host mock, returns false)
  bool setTime(int64_t unixTime) override;

  // Fills struct tm for UTC time. Returns true on success.
  bool getLocalTime(struct tm *result) override;

  // Triggers time sync (no-op for host mock since we use system time)
  bool syncTime(const char *ntpServer) override;

  // Returns true since host mock always uses system time
  bool isTimeSynced() override;

  // Returns the last successful sync time (startup time for host mock)
  int64_t getLastSyncTime() override;

  // Returns the time when this Time object was created (application start)
  int64_t getStartTime() const;

private:
  std::chrono::system_clock::time_point startTime;
  std::atomic<bool> timeSynced{true}; // Always true for host mock
};