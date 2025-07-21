#include "Time.h"
#include <ctime>
#include <cstring>

Time::Time() {
  // Record the time when this object is created (application start time)
  startTime = std::chrono::system_clock::now();
}

int64_t Time::getTime() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

bool Time::setTime(int64_t unixTime) {
  // Not supported on host mock - we use system time
  return false;
}

bool Time::getLocalTime(struct tm *result) {
  if (!result) {
    return false;
  }

  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  
  // Use gmtime_r for thread safety, fallback to gmtime if not available
#ifdef _WIN32
  if (gmtime_s(result, &time_t_now) == 0) {
    return true;
  }
#else
  if (gmtime_r(&time_t_now, result) != nullptr) {
    return true;
  }
#endif

  // Fallback (not thread-safe)
  struct tm *tm_ptr = gmtime(&time_t_now);
  if (tm_ptr) {
    *result = *tm_ptr;
    return true;
  }

  return false;
}

bool Time::syncTime(const char *ntpServer) {
  // Host mock doesn't need to sync - we use system time
  // In a real implementation, this would trigger NTP sync
  return true;
}

bool Time::isTimeSynced() {
  // Host mock always considers time synced since we use system time
  return timeSynced.load();
}

int64_t Time::getLastSyncTime() {
  // Host mock considers start time as sync time
  return getStartTime();
}

int64_t Time::getStartTime() const {
  auto duration = startTime.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

bool Time::getUTCTime(int64_t unixTime, struct tm* result) {
  if (!result) return false;
  time_t timeVal = static_cast<time_t>(unixTime);
  return gmtime_r(&timeVal, result) != nullptr;
}

int Time::getCurrentUTCHour() {
  struct tm utc_tm;
  int64_t now = getTime();
  if (!getUTCTime(now, &utc_tm)) {
    return 0;
  }
  return utc_tm.tm_hour;
}

int Time::getUTCHour(int64_t unixTime) {
  struct tm utc_tm;
  if (!getUTCTime(unixTime, &utc_tm)) {
    return 0;
  }
  return utc_tm.tm_hour;
}

const char* Time::formatTimeISO(int64_t unixTime) {
  struct tm utc_tm;
  if (!getUTCTime(unixTime, &utc_tm)) {
    static const char* error = "1970-01-01T00:00:00Z";
    return error;
  }
  
  static char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
  return buffer;
}