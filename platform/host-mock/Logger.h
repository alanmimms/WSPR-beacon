#pragma once

#include "LoggerIntf.h"

class Logger : public LoggerIntf {
public:
  Logger();
  ~Logger() override;

  // Legacy single-message methods
  void logInfo(const char *msg) override;
  void logWarn(const char *msg) override;
  void logError(const char *msg) override;
  void logDebug(const char *msg) override;
  
  // Printf-style methods with subsystem tag
  void logInfo(const char *tag, const char *format, ...) override;
  void logWarn(const char *tag, const char *format, ...) override;
  void logError(const char *tag, const char *format, ...) override;
  void logDebug(const char *tag, const char *format, ...) override;
  
  // Vsprintf versions
  void vlogInfo(const char *tag, const char *format, va_list args) override;
  void vlogWarn(const char *tag, const char *format, va_list args) override;
  void vlogError(const char *tag, const char *format, va_list args) override;
  void vlogDebug(const char *tag, const char *format, va_list args) override;
};