#include "Time.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <sys/time.h>
#include <cstring>

static const char* TAG = "Time";

Time* Time::instance = nullptr;

Time::Time() {
  instance = this;
  bootTime = std::chrono::steady_clock::now();
  initializeSNTP();
}

int64_t Time::getTime() {
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) == 0) {
    return tv.tv_sec;
  }
  return 0;
}

bool Time::setTime(int64_t unixTime) {
  struct timeval tv;
  tv.tv_sec = static_cast<time_t>(unixTime);
  tv.tv_usec = 0;
  
  if (settimeofday(&tv, nullptr) == 0) {
    ESP_LOGI(TAG, "System time set to: %lld", (long long)unixTime);
    return true;
  }
  
  ESP_LOGE(TAG, "Failed to set system time");
  return false;
}

bool Time::getLocalTime(struct tm *result) {
  if (!result) {
    return false;
  }

  time_t now = getTime();
  if (now == 0) {
    return false;
  }
  
  // Use gmtime_r for thread safety (ESP-IDF supports this)
  if (gmtime_r(&now, result) != nullptr) {
    return true;
  }

  return false;
}

bool Time::syncTime(const char *ntpServer) {
  if (!ntpServer) {
    ESP_LOGE(TAG, "NTP server name is null");
    return false;
  }

  ESP_LOGI(TAG, "Starting SNTP sync with server: %s", ntpServer);
  
  // Stop any existing SNTP service
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  
  // Configure SNTP
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, ntpServer);
  esp_sntp_set_time_sync_notification_cb(sntpTimeSyncNotificationCallback);
  esp_sntp_init();
  
  ESP_LOGI(TAG, "SNTP sync initiated");
  return true;
}

bool Time::isTimeSynced() {
  return timeSynced.load();
}

int64_t Time::getStartTime() {
  // If we have a first sync time, use that as the effective boot time
  int64_t firstSync = firstSyncTime.load();
  if (firstSync > 0) {
    return firstSync;
  }
  
  // Otherwise, estimate based on boot time and current time
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - bootTime);
  
  int64_t currentTime = getTime();
  if (currentTime > 0) {
    return currentTime - elapsed.count();
  }
  
  // Fallback: return 0 if no valid time available
  return 0;
}

void Time::sntpTimeSyncNotificationCallback(struct timeval *tv) {
  if (instance && tv) {
    ESP_LOGI(TAG, "SNTP time synchronized: %lld", (long long)tv->tv_sec);
    instance->timeSynced.store(true);
    
    // Store the first sync time as our reference boot time
    int64_t currentFirstSync = instance->firstSyncTime.load();
    if (currentFirstSync == 0) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - instance->bootTime);
      int64_t bootTime = tv->tv_sec - elapsed.count();
      instance->firstSyncTime.store(bootTime);
      ESP_LOGI(TAG, "Calculated boot time: %lld", (long long)bootTime);
    }
  }
}

void Time::initializeSNTP() {
  ESP_LOGI(TAG, "Initializing SNTP client");
  
  // Set timezone to UTC
  setenv("TZ", "UTC", 1);
  tzset();
  
  // SNTP will be started explicitly when syncTime() is called
  ESP_LOGI(TAG, "SNTP client initialized (not started)");
}