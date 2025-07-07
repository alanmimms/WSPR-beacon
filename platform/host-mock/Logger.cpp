#include "Logger.h"
#include <cstdio>
#include <cstdarg>

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

// Vsprintf versions
void Logger::vlogInfo(const char *tag, const char *format, va_list args) {
  printf("[INFO] [%s] ", tag);
  vprintf(format, args);
  printf("\n");
}

void Logger::vlogWarn(const char *tag, const char *format, va_list args) {
  printf("[WARN] [%s] ", tag);
  vprintf(format, args);
  printf("\n");
}

void Logger::vlogError(const char *tag, const char *format, va_list args) {
  printf("[ERROR] [%s] ", tag);
  vprintf(format, args);
  printf("\n");
}

void Logger::vlogDebug(const char *tag, const char *format, va_list args) {
  printf("[DEBUG] [%s] ", tag);
  vprintf(format, args);
  printf("\n");
}