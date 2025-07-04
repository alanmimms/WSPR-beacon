#include "Logger.h"
#include <esp_log.h>

Logger::Logger() {}

Logger::~Logger() {}

void Logger::logInfo(const char *msg) {
  ESP_LOGI("APP", "%s", msg);
}

void Logger::logWarn(const char *msg) {
  ESP_LOGW("APP", "%s", msg);
}

void Logger::logError(const char *msg) {
  ESP_LOGE("APP", "%s", msg);
}

void Logger::logDebug(const char *msg) {
  ESP_LOGD("APP", "%s", msg);
}