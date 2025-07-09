#include "Logger.h"
#include <esp_log.h>
#include <cstdio>
#include <cstring>

static const char tag[] = "App";

Logger::Logger() {}

Logger::~Logger() {}

void Logger::logInfo(const char *msg) {
  ESP_LOGI(tag, "%s", msg);
}

void Logger::logWarn(const char *msg) {
  ESP_LOGW(tag, "%s", msg);
}

void Logger::logError(const char *msg) {
  ESP_LOGE(tag, "%s", msg);
}

void Logger::logDebug(const char *msg) {
  ESP_LOGD(tag, "%s", msg);
}

// Printf-style methods with subsystem tag
void Logger::logInfo(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vlogInfo(tag, format, args);
  va_end(args);
}

void Logger::logWarn(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vlogWarn(tag, format, args);
  va_end(args);
}

void Logger::logError(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vlogError(tag, format, args);
  va_end(args);
}

void Logger::logDebug(const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vlogDebug(tag, format, args);
  va_end(args);
}

// Vsprintf versions - use ESP-IDF logging macros with buffer
void Logger::vlogInfo(const char *tag, const char *format, va_list args) {
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  ESP_LOGI(tag, "%s", buffer);
}

void Logger::vlogWarn(const char *tag, const char *format, va_list args) {
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  ESP_LOGW(tag, "%s", buffer);
}

void Logger::vlogError(const char *tag, const char *format, va_list args) {
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  ESP_LOGE(tag, "%s", buffer);
}

void Logger::vlogDebug(const char *tag, const char *format, va_list args) {
  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  ESP_LOGD(tag, "%s", buffer);
}
