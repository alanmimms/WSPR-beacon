#pragma once

#include <cstdarg>

class LoggerIntf {
public:
  virtual ~LoggerIntf() {}

  // Legacy single-message methods
  virtual void logInfo(const char *msg) = 0;
  virtual void logWarn(const char *msg) = 0;
  virtual void logError(const char *msg) = 0;
  virtual void logDebug(const char *msg) = 0;
  
  // Printf-style methods with subsystem tag (like ESP_LOGI)
  virtual void logInfo(const char *tag, const char *format, ...) = 0;
  virtual void logWarn(const char *tag, const char *format, ...) = 0;
  virtual void logError(const char *tag, const char *format, ...) = 0;
  virtual void logDebug(const char *tag, const char *format, ...) = 0;
  
  // For implementation classes to use
  virtual void vlogInfo(const char *tag, const char *format, va_list args) = 0;
  virtual void vlogWarn(const char *tag, const char *format, va_list args) = 0;
  virtual void vlogError(const char *tag, const char *format, va_list args) = 0;
  virtual void vlogDebug(const char *tag, const char *format, va_list args) = 0;
};