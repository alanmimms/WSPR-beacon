#include "Logger.h"
#include <cstdio>

Logger::Logger() {}

Logger::~Logger() {}

void Logger::logInfo(const char *msg) {
  printf("[INFO] %s\n", msg);
}

void Logger::logWarn(const char *msg) {
  printf("[WARN] %s\n", msg);
}

void Logger::logError(const char *msg) {
  printf("[ERROR] %s\n", msg);
}

void Logger::logDebug(const char *msg) {
  printf("[DEBUG] %s\n", msg);
}